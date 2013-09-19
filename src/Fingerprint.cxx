//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include "Fingerprint.h"
#include "Params.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include "win_funcs.h"
#endif

unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed ) {
    // MurmurHash2, by Austin Appleby http://sites.google.com/site/murmurhash/
    // m and r are constants set by austin
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    // Initialize the hash to a 'random' value
    unsigned int h = seed ^ len;
    // Mix 4 bytes at a time into the hash
    const unsigned char * data = (const unsigned char *)key;
    while(len >= 4)    {
        unsigned int k = *(unsigned int *)data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch(len)    {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

Fingerprint::Fingerprint(SubbandAnalysis* pSubbandAnalysis, int offset, int numSamples, int codeType, bool inSession)
    : _pSubbandAnalysis(pSubbandAnalysis),_InSession(inSession), _CodeType(codeType), _Offset(offset),_NumSamples(numSamples) {_PrevNumSamples = 0; }


uint Fingerprint::adaptiveOnsets(int ttarg, matrix_u&out, uint*&onset_counter_for_band, uint*&onset_counter_for_band_tot) {
    //  E is a sgram-like matrix of energies.
    const float *pE;
    int bands, frames, i, j, k;
    int deadtime = 128;
    double H[SUBBANDS],taus[SUBBANDS], N[SUBBANDS];
    int contact[SUBBANDS], lcontact[SUBBANDS], tsince[SUBBANDS];
    double overfact = 1.1;  /* threshold rel. to actual peak */
    uint onset_counter = 0;
    uint onset_counter_tot = 0;

    matrix_f E = _pSubbandAnalysis->getMatrix();

    // Take successive stretches of 8 subband samples and sum their energy under a hann window, then hop by 4 samples (50% window overlap).
    int hop = 4;
    int nsm = 8;
    float ham[8];
    //create hann window
    for(int i = 0 ; i != nsm ; i++)
        ham[i] = .5 - .5*cos( (2.*M_PI/(nsm-1))*i);

//    printf("E.size: %d, hop: %d, nsm: %d\n", (int)E.size2(), hop, nsm);
    int nc =  floor((float)E.size2()/(float)hop)-(floor((float)nsm/(float)hop)-1);
    //int nc =  floor((float)E.size2()/(float)hop);
    matrix_f Eb = matrix_f(nc, 8);
    for(uint r=0;r<Eb.size1();r++) for(uint c=0;c<Eb.size2();c++) Eb(r,c) = 0.0;

//    printf("nc: %d\n", nc);
    for(i=0;i<nc;i++) {
        for(j=0;j<SUBBANDS;j++) {
            for(k=0;k<nsm;k++)  Eb(i,j) = Eb(i,j) + ( E(j,(i*hop)+k) * ham[k]);
            Eb(i,j) = sqrtf(Eb(i,j));
        }
    }

    frames = Eb.size1();
    bands = Eb.size2();
    pE = &Eb.data()[0];
    int prevFrames = 0;
    int temp;
    _PrevOffset = 0;
    _PrevNumSamples = 0;
//    printf("Frames: %d, samples: %d\n", frames, _NumSamples);
    if (_InSession) {
        FILE *outF = fopen("outMat.tmp","r");
        if (outF == NULL){
            printf("error reading from memory temp file. Initializing to 1.\n");
        }
        fscanf(outF, "%d ", &prevFrames);
        fscanf(outF, "%d ", &_PrevNumSamples);
        fscanf(outF, "%d ", &_PrevOffset);

        out = matrix_u(SUBBANDS, frames+prevFrames);
    
        for (i=0;i<prevFrames;i++){
            for (j=0;j<SUBBANDS;j++){
                fscanf(outF, "%d ",&temp);
                out(j,i)=temp;
            }
        }
        fclose(outF);
    }
    else{
        out = matrix_u(SUBBANDS, frames);
    }
    _PrevFrames = prevFrames;
//    printf("PevFrames: %d, PrevSamples: %d\n", prevFrames, _PrevNumSamples);
    onset_counter_for_band = new uint[SUBBANDS];

    onset_counter_for_band_tot = new uint[SUBBANDS];
    
    double bn[] = {0.1883, 0.4230, 0.3392}; /* preemph filter */   // new
    int nbn = 3;
    double a1 = 0.98;
    double Y0[SUBBANDS];

    for (j = 0; j < bands; ++j) {
        onset_counter_for_band[j] = 0;
        onset_counter_for_band_tot[j] = 0;
        N[j] = 0.0;
        taus[j] = 1.0;
        H[j] = pE[j];
        contact[j] = 0;
        lcontact[j] = 0;
        tsince[j] = 0;
        Y0[j] = 0;
    }
    
    if (_InSession) {
        FILE *f = fopen("mem.tmp", "r");
        if (f == NULL){
            printf("error reading from memory temp file. Initializing to 1.\n");
        }
        else{
            for (j = 0; j < bands; ++j){
     //           fscanf(f,"%lf %lf %lf %d %d %d %lf ",
                fscanf(f,"%d ",&onset_counter_for_band_tot[j]);
                fscanf(f,"%lf ",&N[j]);
                fscanf(f,"%lf ",&taus[j]);
//                fscanf(f,"%lf ",&H[j]);
                fscanf(f,"%d ",&lcontact[j]);
                fscanf(f,"%d ",&tsince[j]);
                fscanf(f,"%lf ",&Y0[j]);
            }
            fscanf(f, "%d ", &onset_counter_tot);
            fclose(f);
        }
    }

    /*  
    for (j = 0; j < SUBBANDS; ++j) {
        printf("\n%d\n %lf\n %lf\n %lf\n %d\n %d\n %d\n %lf\n ",
                onset_counter_for_band_tot[j],
                N[j],
                taus[j],
                H[j],
                contact[j],
                lcontact[j],
                tsince[j],
                Y0[j]);
    }
    */

//    printf("prev frames %d\n", prevFrames);
    for (i = 0; i < frames; ++i) {
        for (j = 0; j < SUBBANDS; ++j) {
        
                
            double xn = 0;
            /* calculate the filter -  FIR part */
            if (i >= 2*nbn) {
                for (int k = 0; k < nbn; ++k) {
                    xn += bn[k]*(pE[j-SUBBANDS*k] - pE[j-SUBBANDS*(2*nbn-k)]);
                }
            }
            /* IIR part */
            xn = xn + a1*Y0[j];
            /* remember the last filtered level */
            Y0[j] = xn;

            contact[j] = (xn > H[j])? 1 : 0;

            if (contact[j] == 1 && lcontact[j] == 0) {
                /* attach - record the threshold level unless we have one */
                if(N[j] == 0) {
                    N[j] = H[j];
                }
            }
            if (contact[j] == 1) {
                /* update with new threshold */
                H[j] = xn * overfact;
            } else {
                /* apply decays */
                H[j] = H[j] * exp(-1.0/(double)taus[j]);
            }

            if (contact[j] == 0 && lcontact[j] == 1) {
                /* detach */
                if (onset_counter_for_band_tot[j] > 0   && (int)out(j, onset_counter_for_band_tot[j]-1) > i+prevFrames - deadtime) {
                    // overwrite last-written time
                    --onset_counter_for_band[j];
                    --onset_counter;
                    --onset_counter_for_band_tot[j];
                    --onset_counter_tot;
                }
                //printf("onset here\n");
                onset_counter_for_band[j]++;
                out(j, onset_counter_for_band_tot[j]++) = i+prevFrames;
                ++onset_counter;
                ++onset_counter_tot;
                tsince[j] = 0;
            }
            ++tsince[j];
            if (tsince[j] > ttarg) {
                taus[j] = taus[j] - 1;
                if (taus[j] < 1) taus[j] = 1;
            } else {
                taus[j] = taus[j] + 1;
            }

            if ( (contact[j] == 0) &&  (tsince[j] > deadtime)) {
                /* forget the threshold where we recently hit */
                N[j] = 0;
            }
            lcontact[j] = contact[j];
        }
        pE += bands;
    }
    
    
    //save taus for next round
    FILE *fout = fopen("mem.tmp", "w");
    if (fout==NULL){
        printf("Error opening temp file for memory.\n");
        exit(1);
    }
    for (j=0;j<bands;++j) {
    //    fprintf(fout,"%lf %lf %lf %d %d %d %lf ",
        fprintf(fout,"%d ",onset_counter_for_band_tot[j]);
//        printf("opb       %d\n",onset_counter_for_band_tot[j]);
//        printf("opb total %d \n",onset_counter_for_band[j]);
        fprintf(fout,"%lf ",N[j]);
        fprintf(fout,"%lf ",taus[j]);
//        fprintf(fout,"%lf ",H[j]);
        fprintf(fout,"%d ",lcontact[j]);
        fprintf(fout,"%d ",tsince[j]);
        fprintf(fout,"%lf ",Y0[j]);
//        printf("%d, %d\n", onset_counter_for_band[j], onset_counter_for_band_tot[j]);
    }
    fprintf(fout, "%d ", onset_counter_tot);
//    printf("OC     %d \n", onset_counter);
//    printf("OC tot %d \n", onset_counter_tot);
    fclose(fout);
    //printf("%d\n", onset_counter_tot);

    //save out matrix for next round
    FILE *outOutF = fopen("outMat.tmp","w");
    if (outOutF == NULL){
        printf("error reading from memory temp file. Initializing to 1\n");
    }
    fprintf(outOutF, "%d ", frames+prevFrames);
    fprintf(outOutF, "%d ", _NumSamples + _PrevNumSamples);
    fprintf(outOutF, "%d ", _Offset);
    
 //   printf("before %d\n", _Offset);
    if (_InSession) {
        _Offset = _PrevOffset;
    }
    
//    printf("after %d\n", _Offset);
    for (i=0;i<prevFrames+frames;i++){
        for (j=0;j<SUBBANDS;j++){
            fprintf(outOutF, "%d ",out(j,i));
//            printf("%d ", out(j,i));
        }
  //      printf("\n", out(j,i));
    }
    fclose(outOutF);

    return onset_counter_tot;
}


// dan is going to beat me if i call this "decimated_time_for_frame" like i want to
uint Fingerprint::quantized_time_for_frame_delta(uint frame_delta) {
    double time_for_frame_delta = (double)frame_delta / ((double)Params::AudioStreamInput::SamplingRate / 32.0);
    return ((int)floor((time_for_frame_delta * 1000.0) / (float)QUANTIZE_DT_S) * QUANTIZE_DT_S) / floor(QUANTIZE_DT_S*1000.0);
}

uint Fingerprint::quantized_time_for_frame_absolute(uint frame) {
    double time_for_frame = ((double)_Offset)/((double)Params::AudioStreamInput::SamplingRate) + (double)frame / ((double)Params::AudioStreamInput::SamplingRate / 32.0);
    return ((int)rint((time_for_frame * 1000.0) /  (float)QUANTIZE_A_S) * QUANTIZE_A_S) / floor(QUANTIZE_A_S*1000.0);
}


void Fingerprint::Compute() {
    int ttarg;
    uint actual_codes = 0;
    unsigned char hash_material[5];
    for(uint i=0;i<5;i++) hash_material[i] = 0;
    uint * onset_counter_for_band;
    uint * onset_counter_for_band_tot;
    matrix_u out;
    //ttarg orig 345
    if (_CodeType==2){ 
        ttarg=100;
    }
    else {
        ttarg=345;
    }

    uint onset_count = adaptiveOnsets(ttarg, out, onset_counter_for_band, onset_counter_for_band_tot);
    _Codes.resize(onset_count*6);
    int onset_offset =0;
    for(unsigned char band=0;band<SUBBANDS;band++) {
        if (onset_counter_for_band_tot[band]>2) {
            if (_InSession){
                //onset_offset = onset_counter_for_band_tot[band]- onset_counter_for_band[band];
            }

            for(uint onset=0;onset<onset_counter_for_band_tot[band]-2;onset++) {
    //printf("place: %d, %d\n", band, onset);
                // What time was this onset at?
                uint time_for_onset_ms_quantized = quantized_time_for_frame_absolute(out(band,onset+onset_offset));

                uint p[2][6];
                for (int i = 0; i < 6; i++) {
                    p[0][i] = 0;
                    p[1][i] = 0;
                }
                int nhashes = 6;

                if ((int)onset+onset_offset == (int)onset_counter_for_band_tot[band]-4)  { nhashes = 3; }
                if ((int)onset+onset_offset == (int)onset_counter_for_band_tot[band]-3)  { nhashes = 1; }
                p[0][0] = (out(band,onset+1+onset_offset) - out(band,onset+onset_offset));
                p[1][0] = (out(band,onset+2+onset_offset) - out(band,onset+1+onset_offset));
                if(nhashes > 1) {
                    p[0][1] = (out(band,onset+1+onset_offset) - out(band,onset+onset_offset));
                    p[1][1] = (out(band,onset+3+onset_offset) - out(band,onset+1+onset_offset));
                    p[0][2] = (out(band,onset+2+onset_offset) - out(band,onset+onset_offset));
                    p[1][2] = (out(band,onset+3+onset_offset) - out(band,onset+2+onset_offset));
                    if(nhashes > 3) {
                        p[0][3] = (out(band,onset+1+onset_offset) - out(band,onset+onset_offset));
                        p[1][3] = (out(band,onset+4+onset_offset) - out(band,onset+1+onset_offset));
                        p[0][4] = (out(band,onset+2+onset_offset) - out(band,onset+onset_offset));
                        p[1][4] = (out(band,onset+4+onset_offset) - out(band,onset+2+onset_offset));
                        p[0][5] = (out(band,onset+3+onset_offset) - out(band,onset+onset_offset));
                        p[1][5] = (out(band,onset+4+onset_offset) - out(band,onset+3+onset_offset));
                    }
                }

                // For each pair emit a code
                for(uint k=0;k<6;k++) {
                    // Quantize the time deltas to 23ms
                    short time_delta0 = (short)quantized_time_for_frame_delta(p[0][k]);
                    short time_delta1 = (short)quantized_time_for_frame_delta(p[1][k]);
                    // Create a key from the time deltas and the band index
                    memcpy(hash_material+0, (const void*)&time_delta0, 2);
                    memcpy(hash_material+2, (const void*)&time_delta1, 2);
                    memcpy(hash_material+4, (const void*)&band, 1);
                    uint hashed_code = MurmurHash2(&hash_material, 5, HASH_SEED) & HASH_BITMASK;

                    // Set the code alongside the time of onset
                    _Codes[actual_codes++] = FPCode(time_for_onset_ms_quantized, hashed_code);
//                    printf("whee %d,%d: [%d, %d] (%d, %d), %d = %u at %d\n", actual_codes, k, time_delta0, time_delta1, p[0][k], p[1][k], band, hashed_code, time_for_onset_ms_quantized);
                }
            }
        }
    }

    _Codes.resize(actual_codes);
    delete [] onset_counter_for_band;
    delete [] onset_counter_for_band_tot;
}


