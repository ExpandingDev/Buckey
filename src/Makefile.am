bin_PROGRAMS = buckey
buckey_SOURCES = core/Mode.cpp core/Service.cpp core/PromptResult.cpp core/EventData.cpp core/PromptEventData.cpp core/OutputEventData.cpp core/ModeControlEventData.cpp core/ServiceControlEventData.cpp core/EventSource.cpp \
core/DynamicGrammar.cpp core/EchoMode.cpp core/CoreMode.cpp \
tts/SpeechPreparedEventData.cpp tts/AsyncSpeechRequestEventData.cpp tts/TTSService.cpp tts/MimicTTSService.cpp \
filters/StringHelper.cpp filters/TextFilter.cpp filters/PerWordSingleReplacementFilter.cpp \
sphinx/HypothesisEventData.cpp sphinx/SphinxDecoder.cpp sphinx/SphinxService.cpp sphinx/SphinxMode.cpp \
core/Buckey.cpp main.cpp

SUBDIRS = . core tts sphinx filters


#Includes for main program

buckey_CPPFLAGS = -I$(top_srcdir)/include/ -I$(top_srcdir)/include/core/ -I$(top_srcdir)/include/filters/ -I$(top_srcdir)/include/tts/ -I$(top_srcdir)/include/sphinx/ $(SPHINX_INCLUDE) $(MIMIC_INCLUDE) -I$(JSGF_CPP_INCLUDE) -I$(CPPFS_INCLUDE)
AM_LDFLAGS = -lpthread $(SPHINXLD) $(YAMLLD) $(MIMICLD) $(CPPFSLD)
LDADD = $(JSGFLD) $(MIMICLD)