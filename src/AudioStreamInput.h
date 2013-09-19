//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#ifndef AUDIOSTREAMINPUT_H
#define AUDIOSTREAMINPUT_H
#include "Common.h"
#include "Params.h"
#include <iostream>
#include <string>
#include <math.h>
#include "File.h"
#include <time.h>
#if defined(_WIN32) && !defined(__MINGW32__)
#define snprintf _snprintf
#define DEVNULL "nul"
#else
#define DEVNULL "/dev/null"
#endif

class AudioStreamInput {
public:
    AudioStreamInput();
    virtual ~AudioStreamInput();
    virtual bool ProcessFile(const char* filename, int offset_s=0, int seconds=0);
    virtual bool ProcessFile_alt(const char* filename, int offset_samples=0, int dur_samples=0); 
    virtual std::string GetName() = 0;
    bool ProcessRawFile(const char* rawFilename);
    bool ProcessStandardInput(void);
    bool ProcessFilePointer(FILE* pFile);
    int getNumSamples() const {return _NumberSamples;}
    const float* getSamples() {return _pSamples;}
    void setNumSamples(int numSamples) {_NumberSamples = numSamples;}
    double getDuration() { return (double)getNumSamples() / Params::AudioStreamInput::SamplingRate; }
    virtual bool IsSupported(const char* pFileName); //Everything ffmpeg can do, by default
    int GetOffset() const { return _Offset_s;}
    int GetSeconds() const { return _Seconds;}
protected:

    virtual std::string GetCommandLine(const char* filename) = 0;
    static bool ends_with(const char *s, const char *ends_with);
    float* _pSamples;
    uint _NumberSamples;
    int _Offset_s;
    int _Offset_samples;
    int _Seconds;
    int _Dur_samples;
    char _TempFilename[256];

};

class StdinStreamInput : public AudioStreamInput {
public:
    std::string GetName(){return "stdin";};
protected:
    bool IsSupported(const char* pFileName){ return (std::string("stdin") == pFileName);};
    bool ProcessFile(const char* filename){ return ProcessStandardInput();}
    virtual std::string GetCommandLine(const char* filename){return "";} // hack
};

class FfmpegStreamInput : public AudioStreamInput {
public:
    std::string GetName(){return "ffmpeg";};
protected:
    std::string GetCommandLine(const char* filename) {
        // TODO: Windows
        char message[4096] = {0};
        time_t rawtime;
        struct tm * timeinfo;
        char tmpFile [80];
                   
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        
        strftime (tmpFile,80,"tmp-%b%d%S_kl_%H%M.wav",timeinfo);
        getcwd(_TempFilename, 255);
        strcat(_TempFilename, "/");
        
        strcat(_TempFilename, tmpFile);
       // _TempFilename= tmpFile;
        
                             
        //puts (tmpFile);

        if (_Offset_s == 0 && _Seconds == 0)
            snprintf(message, NELEM(message), "sox \"%s\"  -c %d -r %d -b 16 -t wav %s 2>/dev/null ",
                    filename, Params::AudioStreamInput::Channels, (uint) Params::AudioStreamInput::SamplingRate, _TempFilename);
        else
            snprintf(message, NELEM(message), "sox \"%s\"  -c %d -r %d  -b 16 -t wav %s 2>/dev/null",
                    filename, Params::AudioStreamInput::Channels, (uint) Params::AudioStreamInput::SamplingRate, _TempFilename);
        
        return std::string(message);
    }
};

namespace FFMPEG {
    bool IsAudioFile(const char* pFileName);
};

class Mpg123StreamInput : public AudioStreamInput {
public:
    std::string GetName(){return "mpg123";};
protected:
    #define FRAMES_PER_SECOND 38.2813f
    bool IsSupported(const char* pFileName){ return File::ends_with(pFileName, ".mp3");};
    std::string GetCommandLine(const char* filename) {
        char message[4096] = {0};
        if (_Offset_s == 0 && _Seconds == 0)
            snprintf(message, NELEM(message), "mpg123 --quiet --singlemix --stdout --rate %d \"%s\"",
                (uint) Params::AudioStreamInput::SamplingRate, filename);
        else
            snprintf(message, NELEM(message), "mpg123 --quiet --singlemix --stdout --rate %d --skip %d --frames %d \"%s\"",
                (uint) Params::AudioStreamInput::SamplingRate, (uint)(_Offset_s * FRAMES_PER_SECOND) /* unprecise */, (uint)ceilf(_Seconds * FRAMES_PER_SECOND) /* unprecise */, filename);
        return std::string(message);
    }
};

#endif


