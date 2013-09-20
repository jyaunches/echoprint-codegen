//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include "Whitening.h"
#include "AudioStreamInput.h"
#include <stdio.h>
#include <string.h>

Whitening::Whitening(AudioStreamInput* pAudio) {
    _pSamples = pAudio->getSamples();
    _NumSamples = pAudio->getNumSamples();
    Init();
}

Whitening::Whitening(const float* pSamples, uint numSamples, bool inSession, char *path) :
    _pSamples(pSamples), _NumSamples(numSamples), _InSession(inSession), _Path(path) {
    Init();
}

Whitening::~Whitening() {
    free(_R);
    free(_Xo);
    free(_Save_Xo);
    free(_ai);
    free(_whitened);
}

void Whitening::Init() {
    int i;
    _p = 40;

    _R = (float *)malloc((_p+1)*sizeof(float));
    for (i = 0; i <= _p; ++i)  { _R[i] = 0.0; }
    _R[0] = 0.001;

    _Xo = (float *)malloc((_p+1)*sizeof(float));
    for (i = 0; i < _p; ++i)  { _Xo[i] = 0.0; }

    _Save_Xo = (float *)malloc((_p+1)*sizeof(float));
    for (i = 0; i < _p; ++i)  { _Save_Xo[i] = 0.0; }

    _ai = (float *)malloc((_p+1)*sizeof(float));
    _whitened = (float*) malloc(sizeof(float)*_NumSamples);
}

void Whitening::Compute() {
    int blocklen = 10000;
    int i, newblocklen;
    bool first=0; 
    bool last=0;
    int numFrames = (_NumSamples - 128 + 1)/8;
    int nc =  floor((float)numFrames/4.)-(floor(8./4.)-1);
    _NewNumSamples = nc*32;

    for(i=0;i<(int)_NumSamples;i=i+blocklen) {
        if (i==0) first = 1;
        else first = 0;
        if (i+blocklen >= (int)_NewNumSamples) {last = 1;}
        if (i >= (int)_NewNumSamples && i <(int) _NumSamples) {last = 0;}
        if (i+blocklen >= (int)_NumSamples) {
            newblocklen = _NumSamples -i - 1;
        } else { newblocklen = blocklen; }
        ComputeBlock(i, newblocklen, first, last);
    }
}

void Whitening::ComputeBlock(int start, int blockSize, bool first, bool last) {
    int i, j;
    float alpha, E, ki;
    float T = 8;
    alpha = 1.0/T;
    char path[128];
    strcpy(path, _Path);
    strcat(path,"white.tmp" );
        fprintf(stderr, "%s\n", path);

    //retrieve last few frames of input from last session
    if (_InSession && first) {
                FILE *f = fopen(path ,"r");
        if (f == NULL){
            printf("error reading from memory temp file.\n");
        }
        for (i = 0; i <= _p; ++i) {
            fscanf(f, "%f ",&_Xo[i]);
            fscanf(f, "%f ",&_ai[i]);
            fscanf(f, "%f ",&_R[i]);
        }
        fclose(f);
     }

    // calculate autocorrelation of current block
    for (i = 0; i <= _p; ++i) {
        float acc = 0;
        for (j = i; j < (int)blockSize; ++j) {
            acc += _pSamples[j+start] * _pSamples[j-i+start];
        }
        // smoothed update
        _R[i] += alpha*(acc - _R[i]);
    }

    // calculate new filter coefficients
    // Durbin's recursion, per p. 411 of Rabiner & Schafer 1978
    E = _R[0];
    for (i = 1; i <= _p; ++i) {
        float sumalphaR = 0;
        for (j = 1; j < i; ++j) {
            sumalphaR += _ai[j]*_R[i-j];
        }
        ki = (_R[i] - sumalphaR)/E;
        _ai[i] = ki;
        for (j = 1; j <= i/2; ++j) {
            float aj = _ai[j];
            float aimj = _ai[i-j];
            _ai[j] = aj - ki*aimj;
            _ai[i-j] = aimj - ki*aj;
        }
        E = (1-ki*ki)*E;
    }
    // calculate new output
    for (i = 0; i < (int)blockSize; ++i) {
        float acc = _pSamples[i+start];
        int minip = i;
        if (_p < minip) {
            minip = _p;
        }
        
        for (j = i+1; j <= _p; ++j) {
            acc -= _ai[j]*_Xo[_p + i-j];
        }
        for (j = 1; j <= minip; ++j) {
            acc -= _ai[j]*_pSamples[i-j+start];
        }
        _whitened[i+start] = acc;
    }
    // save last few frames of input
    for (i = 0; i <= _p; ++i) {
        _Xo[i] = _pSamples[blockSize-1-_p+i+start];
       if (last) {
           _Save_Xo[i] = _pSamples[_NewNumSamples-1-_p+i];
       } 
    }

    if (last) {
        FILE *outF = fopen(path,"w");
        for (i = 0; i <= _p; ++i) {
            fprintf(outF, "%f ",_Save_Xo[i]);
            fprintf(outF, "%f ",_ai[i]);
            fprintf(outF, "%f ",_R[i]);
        }
        fclose(outF);
    }
}


