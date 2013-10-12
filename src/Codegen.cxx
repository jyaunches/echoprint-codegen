//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include <sstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include "Codegen.h"
#include "AudioBufferInput.h"
#include "Fingerprint.h"
#include "Whitening.h"
#include "SubbandAnalysis.h"
#include "Fingerprint.h"
#include "Common.h"
#include <stdio.h>
#include "Base64.h"
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>

using std::string;
using std::vector;

Codegen::Codegen(const float* pcm, unsigned int numSamples, int start_offset, int codeType, bool inSession, char* path) {
    if (Params::AudioStreamInput::MaxSamples < (uint)numSamples)
        throw std::runtime_error("File was too big\n");
//    fprintf(stderr, "TESTIGTESTINGTESTING\n\n\n");
    if (codeType==1) {
        strcat(path, "ECHO/");
    }
    else {
        strcat(path, "SNAP/");
    }
    
    mode_t process_mask = umask(0);
    mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);
    
    Whitening *pWhitening = new Whitening(pcm, numSamples, inSession, path);
    pWhitening->Compute();
    
    AudioBufferInput *pAudio = new AudioBufferInput();
    pAudio->SetBuffer(pWhitening->getWhitenedSamples(), pWhitening->getNumSamples());

    SubbandAnalysis *pSubbandAnalysis = new SubbandAnalysis(pAudio);
    pSubbandAnalysis->Compute();
    numSamples = pSubbandAnalysis->getNumSamples();
    pAudio->setNumSamples(numSamples);
    _NumSamples = numSamples;

    Fingerprint *pFingerprint = new Fingerprint(pSubbandAnalysis, start_offset, numSamples, codeType, inSession, path);
    pFingerprint->Compute();

    _CodeString = createCodeString(pFingerprint->getCodes());
    _NumCodes = pFingerprint->getCodes().size();

    delete pFingerprint;
    delete pSubbandAnalysis;
    delete pWhitening;
    delete pAudio;
}

string Codegen::createCodeString(vector<FPCode> vCodes) {
    if (vCodes.size() < 3) {
        return "";
    }
    std::ostringstream codestream;
    codestream << std::setfill('0') << std::hex;
    for (uint i = 0; i < vCodes.size(); i++)
        codestream << std::setw(5) << vCodes[i].frame;

    for (uint i = 0; i < vCodes.size(); i++) {
        int hash = vCodes[i].code;
        codestream << std::setw(5) << hash;
    }
    //fprintf(stderr, "\n\n printing code: \n%s\n \n ", codestream.str().c_str() );
    return compress(codestream.str());
}


string Codegen::compress(const string& s) {
    long max_compressed_length = s.size()*2;
    unsigned char *compressed = new unsigned char[max_compressed_length];

    // zlib the code string
    z_stream stream;
    stream.next_in = (Bytef*)(unsigned char*)s.c_str();
    stream.avail_in = (uInt)s.size();
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    do {
        stream.next_out = compressed;
        stream.avail_out = max_compressed_length;
        if(deflate(&stream, Z_FINISH) == Z_STREAM_END) break;
    } while (stream.avail_out == 0);
    uint compressed_length = stream.total_out;
    deflateEnd(&stream);

    // base64 the zlib'd code string
    string encoded = base64_encode(compressed, compressed_length, true);
    delete [] compressed;
    return encoded;
}
