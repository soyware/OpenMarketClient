CXX=g++
CXXFLAGS=-std=c++17 -O2 -Wall -D_FORTIFY_SOURCE=2 -fstack-protector-all
LDLIBS=-lpthread -lcurl -lwolfssl -lstdc++fs
INCLUDES=-I../libs/wolfssl -I../libs/curl/include -I../libs/rapidjson/include
TARGET=market
OUTDIR=build/linux

all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OUTDIR)/stdafx.h.gch \
	$(TARGET)/Captcha.h \
	$(TARGET)/Config.h \
	$(TARGET)/Curl.h \
	$(TARGET)/Guard.h \
	$(TARGET)/Login.h \
	$(TARGET)/Market.h \
	$(TARGET)/Misc.h \
	$(TARGET)/Offer.h \
	$(TARGET)/PKCS7.h \
	$(TARGET)/Main.cpp
	mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -include $(OUTDIR)/stdafx.h $(INCLUDES) $(TARGET)/Main.cpp -o $@ $(LDLIBS)

$(OUTDIR)/stdafx.h.gch: $(TARGET)/stdafx.h
	mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TARGET)/stdafx.h -o $@

clean:
	rm -rf $(OUTDIR)