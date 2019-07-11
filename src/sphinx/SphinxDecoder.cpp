#include "sphinx/SphinxDecoder.h"

SphinxDecoder::SphinxDecoder(std::string decoderName, ps_decoder_t * decoder) {
    name = decoderName;
    state.store(SphinxHelper::DecoderState::NOT_INITIALIZED);
    ready = false;
    ps = decoder;
}

SphinxDecoder::~SphinxDecoder()
{
	ready = false;
	state = SphinxHelper::DecoderState::NOT_INITIALIZED;
    ps_free(ps);
}

ps_decoder_t * SphinxDecoder::getDecoder() {
	return ps;
}

std::string SphinxDecoder::getName() {
	return name;
}

/// Returns true if speech was detected in the last frame
bool SphinxDecoder::processRawAudio(int16 adbuf[], int32 frameCount) {
    ps_process_raw(ps, adbuf, frameCount, FALSE, FALSE);
    return ps_get_in_speech(ps);
}

const bool SphinxDecoder::isReady() {
    return ready;
}

const SphinxHelper::DecoderState SphinxDecoder::getState() {
	return state.load();
}
