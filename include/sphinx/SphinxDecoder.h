#ifndef SPHINXDECODER_H
#define SPHINXDECODER_H
#include <string>
#include <iostream>
#include <atomic>
#include "syslog.h"

#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include "pocketsphinx.h"
#include "cmd_ln.h"

#include "SphinxHelper.h"

#define DEFAULT_HMM_PATH "/usr/local/share/pocketsphinx/model/en-us/en-us"
#define DEFAULT_DICT_PATH "/usr/local/share/pocketsphinx/model/en-us/cmudict-en-us.dict"
#define DEFAULT_LM_PATH "/usr/local/share/pocketsphinx/model/en-us/en-us.lm.bin"
#define DEFAULT_LOG_PATH "/dev/null"

///\brief This class serves as a bare bones C++ wrapper for the CMU pocketsphinx library with a few added convenience functions.
///
///This class serves as a bare bones C++ wrapper for the CMU pocketsphinx library with a few added convenience functions.
///All functions (and constructors and destructors) are synchronous. Any asynchronous tasks should be carried out by a managing class (SphinxRecognizer).
class SphinxDecoder
{
    friend class SphinxService;
    public:
        /// Create a new Sphinx Decoder to wrap a CMU sphinx decoder.
        SphinxDecoder(std::string name, ps_decoder_t * decoder);
        ~SphinxDecoder();

        ///\brief Returns the name of the Decoder.
        std::string getName();

        ///\brief Returns true if the Decoder has an utterance started and is ready to receive audio frames.
        const bool isReady();

        void setDecoder(ps_decoder_t * decoder);
        ps_decoder_t * getDecoder();

        ///\brief Method to pass audio frames to the Decoder.
        ///adbuf should point to an array of audio frames, frameCount is the number of adbuf elements.
        bool processRawAudio(int16 adbuf[], int32 frameCount);

        //Search mode
        std::string getSearchMode();
        void setSearchMode(std::string mode);

        //Getters
        const SphinxHelper::DecoderState getState();

    protected:
    	///Name of this decoder.
    	std::string name;

		std::atomic<bool> ready;
		std::atomic<bool> inUtterance;

		std::atomic<SphinxHelper::DecoderState> state;

        ///CMU Pocketsphinx decoder.
        ps_decoder_t * ps;
};

#endif // SPHINXDECODER_H
