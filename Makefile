CXX=g++
CXXFLAGS=-std=c++17 -O2 -Wall
LDLIBS=-Wl,-rpath -Wl,/usr/local/lib/ -lpthread -lcurl -lwolfssl -lstdc++fs
INCLUDES=-I../libs/wolfssl -I../libs/curl/include -I../libs/rapidjson/include
SRCFOLDER=src
PCH=Precompiled
OUTDIR=build/linux
TARGET=OpenMarketClient

all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OUTDIR)/$(PCH).h.gch \
	$(SRCFOLDER)/Misc.h \
	$(SRCFOLDER)/Curl.h \
	$(SRCFOLDER)/Crypto.h \
	$(SRCFOLDER)/Steam/Misc.h \
	$(SRCFOLDER)/Steam/Captcha.h \
	$(SRCFOLDER)/Steam/Trade.h \
	$(SRCFOLDER)/Steam/Guard.h \
	$(SRCFOLDER)/Steam/Auth.h \
	$(SRCFOLDER)/Market.h \
	$(SRCFOLDER)/Account.h \
	$(SRCFOLDER)/Main.cpp
	mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -include $(OUTDIR)/$(PCH).h $(INCLUDES) $(SRCFOLDER)/Main.cpp -o $@ $(LDLIBS)

$(OUTDIR)/$(PCH).h.gch: $(SRCFOLDER)/$(PCH).h
	mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCFOLDER)/$(PCH).h -o $@
