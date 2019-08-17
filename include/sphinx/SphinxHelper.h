#ifndef SPHINXHELPER_INCLUDED
#define SPHINXHELPER_INCLUDED
namespace SphinxHelper {
    enum RecognitionSource {
        DEVICE, FILE
    };

    enum SearchMode {
    	JSGF, LM, ALLPHONE
    };

    enum DecoderState {
		UTTERANCE_STARTED, UTTERANCE_ENDING, IDLE, NOT_INITIALIZED, ERROR
    };
}


#endif // SPHINXHELPER_INCLUDED
