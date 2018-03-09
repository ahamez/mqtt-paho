all: service device

sanitized: service_sanitized device_sanitized

service: service.cc common.hh
	clang++ -I. -O2 -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a service.cc -o service

device: device.cc common.hh
	clang++ -I. -O2 -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a device.cc -o device

service_sanitized: service.cc common.hh
	clang++ -I. -O2 -g -fsanitize=address,undefined -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a service.cc -o service

device_sanitized: device.cc common.hh
	clang++ -I. -O2 -g -fsanitize=address,undefined -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a device.cc -o device

clean:
	rm -f service device
