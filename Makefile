ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

ifeq ($(DEBUG),)
	BUILD_DIR ?= build/release
else
	BUILD_DIR ?= build/debug
endif

PREFIX = /usr

ADC_BIN = wb-mqtt-adc
SRC_DIR = src
ADC_SOURCES := $(shell find $(SRC_DIR) -name "*.cpp" -and -not -name main.cpp)
ADC_OBJECTS := $(ADC_SOURCES:%=$(BUILD_DIR)/%.o)

CXXFLAGS = -Wall -std=c++17 -I$(SRC_DIR)
LDFLAGS = -lwbmqtt1 -lpthread

ifeq ($(DEBUG),)
	CXXFLAGS += -O2
else
	CXXFLAGS += -g -O0 --coverage
	LDFLAGS += --coverage
endif

TEST_DIR = test
ADC_TEST_SOURCES := $(shell find $(TEST_DIR) -name "*.cpp")
ADC_TEST_OBJECTS=$(ADC_TEST_SOURCES:%=$(BUILD_DIR)/%.o)
TEST_BIN = wb-mqtt-adc-test
TEST_LDFLAGS = -lgtest

export TEST_DIR_ABS = $(CURDIR)/$(TEST_DIR)

VALGRIND_FLAGS = --error-exitcode=180 -q

COV_REPORT ?= $(BUILD_DIR)/cov
GCOVR_FLAGS := -s --html $(COV_REPORT).html -x $(COV_REPORT).xml
ifneq ($(COV_FAIL_UNDER),)
	GCOVR_FLAGS += --fail-under-line $(COV_FAIL_UNDER)
endif

all: $(BUILD_DIR)/$(ADC_BIN)

# ADC
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(BUILD_DIR)/$(ADC_BIN): $(BUILD_DIR)/$(SRC_DIR)/main.cpp.o $(ADC_OBJECTS)
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN): $(ADC_OBJECTS) $(ADC_TEST_OBJECTS)
	$(CXX) $^ $(LDFLAGS) $(TEST_LDFLAGS) -o $@

test: $(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN)
	rm -f $(TEST_DIR)/*.dat.out
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind $(VALGRIND_FLAGS) $(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		else $(TEST_DIR)/abt.sh show; exit 1; fi; \
	else \
		$(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || { $(TEST_DIR)/abt.sh show; exit 1; } \
	fi
ifneq ($(DEBUG),)
	gcovr $(GCOVR_FLAGS) $(BUILD_DIR)/$(SRC_DIR) $(BUILD_DIR)/$(TEST_DIR)
endif

clean:
	-rm -fr build

install: all
	install -d $(DESTDIR)/var/lib/wb-mqtt-adc/conf.d

	install -Dm0755 $(BUILD_DIR)/$(ADC_BIN) -t $(DESTDIR)$(PREFIX)/bin
	install -Dm0755 generate-system-config.sh -t $(DESTDIR)$(PREFIX)/lib/wb-mqtt-adc

	install -Dm0644 data/config.json $(DESTDIR)$(PREFIX)/share/wb-mqtt-adc/wb-mqtt-adc.conf.default
	install -Dm0644 data/config.json.wb55 $(DESTDIR)$(PREFIX)/share/wb-mqtt-adc/wb-mqtt-adc.conf.wb55
	install -Dm0644 data/config.json.wb61 $(DESTDIR)$(PREFIX)/share/wb-mqtt-adc/wb-mqtt-adc.conf.wb61

	install -Dm0644 data/config.json.devicetree $(DESTDIR)$(PREFIX)/share/wb-mqtt-adc/wb-mqtt-adc.conf.devicetree

	install -Dm0644 data/wb-mqtt-adc.wbconfigs $(DESTDIR)/etc/wb-configs.d/12wb-mqtt-adc

	install -Dm0644 data/wb-mqtt-adc-template.schema.json -t $(DESTDIR)$(PREFIX)/share/wb-mqtt-adc
