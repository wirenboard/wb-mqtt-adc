CXX=$(CROSS_COMPILE)g++
CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -std=c++14 -Os -I. -I./thirdparty/valijson-0.2/include

ADC_SOURCES= 						\
			src/adc_driver.cpp		\
			src/config.cpp			\
			src/sysfs_adc.cpp		\
			src/moving_average.cpp	\
			src/file_utils.cpp		\

ADC_OBJECTS=$(ADC_SOURCES:.cpp=.o)
ADC_BIN=wb-homa-adc
ADC_LIBS= -lwbmqtt1 -lpthread -ljsoncpp 

ADC_TEST_SOURCES= 							\
			$(TEST_DIR)/test_main.cpp		\
			$(TEST_DIR)/moving_average.test.cpp	\
			$(TEST_DIR)/file_utils.test.cpp	\
			$(TEST_DIR)/config.test.cpp	\
			$(TEST_DIR)/sysfs_adc.test.cpp	\

TEST_DIR=test
export TEST_DIR_ABS = $(shell pwd)/$(TEST_DIR)

ADC_TEST_OBJECTS=$(ADC_TEST_SOURCES:.cpp=.o)
TEST_BIN=wb-homa-adc-test
TEST_LIBS=-lgtest


all : $(ADC_BIN)

# ADC
%.o : %.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(ADC_BIN) : src/main.o $(ADC_OBJECTS)
	${CXX} $^ ${ADC_LIBS} -o $@

$(TEST_DIR)/$(TEST_BIN): $(ADC_OBJECTS) $(ADC_TEST_OBJECTS)
	${CXX} $^ $(ADC_LIBS) $(TEST_LIBS) -o $@

test: $(TEST_DIR)/$(TEST_BIN)

clean :
	-rm -f src/*.o $(ADC_BIN)
	-rm -f $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_BIN)


install: all
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib/wb-homa-adc
	install -d $(DESTDIR)/usr/share/wb-homa-adc
	install -d $(DESTDIR)/etc/wb-configs.d
	install -d $(DESTDIR)/etc/wb-homa-adc.conf.d

	install -m 0755  $(ADC_BIN) $(DESTDIR)/usr/bin/$(ADC_BIN)
	install -m 0755  generate-system-config.sh $(DESTDIR)/usr/lib/wb-homa-adc/generate-system-config.sh

	install -m 0644  data/config.json $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.default
	install -m 0644  data/config.json.wb55 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb55
	install -m 0644  data/config.json.wb61 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb61

	install -m 0644  data/config.json.devicetree $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.devicetree

	install -m 0644  data/wb-homa-adc.wbconfigs $(DESTDIR)/etc/wb-configs.d/12wb-homa-adc

	install -m 0644  data/wb-homa-adc.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-homa-adc.schema.json
