ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

CXXFLAGS=-Wall -std=c++14 -Os -I.

ADC_SOURCES= 						\
			src/adc_driver.cpp		\
			src/config.cpp			\
			src/sysfs_adc.cpp		\
			src/moving_average.cpp	\
			src/file_utils.cpp		\

ADC_OBJECTS=$(ADC_SOURCES:.cpp=.o)
ADC_BIN=wb-mqtt-adc
ADC_LIBS= -lwbmqtt1 -lpthread

ADC_TEST_SOURCES= 							\
			$(TEST_DIR)/test_main.cpp		\
			$(TEST_DIR)/moving_average.test.cpp	\
			$(TEST_DIR)/file_utils.test.cpp	\
			$(TEST_DIR)/config.test.cpp	\
			$(TEST_DIR)/sysfs_adc.test.cpp	\

TEST_DIR=test
export TEST_DIR_ABS = $(shell pwd)/$(TEST_DIR)

ADC_TEST_OBJECTS=$(ADC_TEST_SOURCES:.cpp=.o)
TEST_BIN=wb-mqtt-adc-test
TEST_LIBS=-lgtest


all : $(ADC_BIN)

# ADC
%.o : %.cpp
	${CXX} -c $< -o $@ ${CXXFLAGS}

$(ADC_BIN) : src/main.o $(ADC_OBJECTS)
	${CXX} $^ ${ADC_LIBS} -o $@

$(TEST_DIR)/$(TEST_BIN): $(ADC_OBJECTS) $(ADC_TEST_OBJECTS)
	${CXX} $^ $(ADC_LIBS) $(TEST_LIBS) -o $@

test: $(TEST_DIR)/$(TEST_BIN)
	rm -f $(TEST_DIR)/*.dat.out
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		else $(TEST_DIR)/abt.sh show; exit 1; fi; \
    else \
        $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || { $(TEST_DIR)/abt.sh show; exit 1; } \
	fi

clean :
	-rm -f src/*.o $(ADC_BIN)
	-rm -f $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_BIN)


install: all
	install -d $(DESTDIR)/var/lib/wb-mqtt-adc/conf.d

	install -D -m 0755  $(ADC_BIN) $(DESTDIR)/usr/bin/$(ADC_BIN)
	install -D -m 0755  generate-system-config.sh $(DESTDIR)/usr/lib/wb-mqtt-adc/generate-system-config.sh

	install -D -m 0644  data/config.json $(DESTDIR)/usr/share/wb-mqtt-adc/wb-mqtt-adc.conf.default
	install -D -m 0644  data/config.json.wb55 $(DESTDIR)/usr/share/wb-mqtt-adc/wb-mqtt-adc.conf.wb55
	install -D -m 0644  data/config.json.wb61 $(DESTDIR)/usr/share/wb-mqtt-adc/wb-mqtt-adc.conf.wb61

	install -D -m 0644  data/config.json.devicetree $(DESTDIR)/usr/share/wb-mqtt-adc/wb-mqtt-adc.conf.devicetree

	install -D -m 0644  data/wb-mqtt-adc.wbconfigs $(DESTDIR)/etc/wb-configs.d/12wb-mqtt-adc

	install -D -m 0644  data/wb-mqtt-adc.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-adc.schema.json
