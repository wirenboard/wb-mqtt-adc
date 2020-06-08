CXX=$(CROSS_COMPILE)g++
CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -std=c++14 -Os -I.

ADC_SOURCES= 					\
			src/adc_driver.cpp	\
			src/config.cpp		\
			src/sysfs_adc.cpp	\

ADC_OBJECTS=$(ADC_SOURCES:.cpp=.o)
ADC_BIN=wb-homa-adc
ADC_LIBS= -lwbmqtt1 -lpthread -ljsoncpp 

ADC_TEST_SOURCES= 							\
			$(TEST_DIR)/test_main.cpp		\
			$(TEST_DIR)/sysfs_w1_test.cpp	\
    		$(TEST_DIR)/onewire_driver_test.cpp	\
			
TEST_DIR=test
export TEST_DIR_ABS = $(shell pwd)/$(TEST_DIR)

ADC_TEST_OBJECTS=$(ADC_TEST_SOURCES:.cpp=.o)
TEST_BIN=wb-homa-adc-test
TEST_LIBS=-lgtest -lwbmqtt_test_utils


all : $(ADC_BIN)

# ADC
%.o : %.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(ADC_BIN) : src/main.o $(ADC_OBJECTS)
	${CXX} $^ ${ADC_LIBS} -o $@

$(TEST_DIR)/$(TEST_BIN): $(ADC_OBJECTS) $(ADC_TEST_OBJECTS)
	${CXX} $^ $(ADC_LIBS) $(TEST_LIBS) -o $@

test: $(TEST_DIR)/$(TEST_BIN)

	rm -f $(TEST_DIR)/*.dat.out
	if [ "$(shell arch)" = "armv7l" ]; then \
          $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || $(TEST_DIR)/abt.sh show; \
        else \
          valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || \
            if [ $$? = 180 ]; then \
              echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
              exit 1; \
            else $(TEST_DIR)/abt.sh show; exit 1; fi; \
        fi
clean :
	-rm -f src/*.o $(ADC_BIN)
	-rm -f $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_BIN)


install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/usr/share/wb-homa-adc
	mkdir -p $(DESTDIR)/etc/wb-configs.d

	install -m 0755  $(ADC_BIN) $(DESTDIR)/usr/bin/$(ADC_BIN)
	install -m 0644  data/config.json $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.default
	install -m 0644  data/config.json.devicetree $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.devicetree
	install -m 0644  data/config.json.wb55 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb55
	install -m 0644  data/config.json.wb61 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb61

	install -m 0644  data/wb-homa-adc.wbconfigs $(DESTDIR)/etc/wb-configs.d/12wb-homa-adc

	install -m 0644  data/wb-homa-adc.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-homa-adc.schema.json
