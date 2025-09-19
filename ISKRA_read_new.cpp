#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <MQTTClient.h>

// ---------------- MQTT ----------------
#define ADDRESS   "tcp://localhost:1883"
#define CLIENTID  "ISKRA_reader"
#define TOPIC_TOTAAL "/0111"
#define TOPIC_HOOG   "/0112"
#define TOPIC_LAAG   "/0113"
#define TOPIC_WH     "/0114"
#define TOPIC_A      "/0115"

#define QOS      1
#define TIMEOUT  10000L

// ---------------- Data struct ----------------
struct ISKRA {
    double totaal = 0.0;
    double totaal_oud = 0.0;
    double hoog = 0.0;
    double laag = 0.0;
    double verbruik_W = 0.0;
    int verbruik_A = 0;
};

// ---------------- Global ----------------
ISKRA iskra162;

// ---------------- Helpers ----------------
int open_uart(const std::string &device, int baud, bool parity) {
    int fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd == -1) {
        perror("UART open failed");
        return -1;
    }

    struct termios options{};
    tcgetattr(fd, &options);

    options.c_cflag = baud | CS7 | CLOCAL | CREAD;
    if (parity) options.c_cflag |= PARENB;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

bool extractValue(const std::string &src, const std::string &key, double &out) {
    size_t start = src.find(key + "(");
    if(start == std::string::npos) return false;
    size_t end = src.find(")", start);
    if(end == std::string::npos) return false;
    std::string val = src.substr(start + key.size() + 1, end - (start + key.size() + 1));
    try {
        out = std::stod(val);
        return true;
    } catch(...) {
        return false;
    }
}

// ---------------- Main ----------------
int main() {
    // MQTT init
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        std::cerr << "Failed to connect to MQTT, rc=" << rc << std::endl;
        return -1;
    }

    std::cout << "Connected to MQTT broker." << std::endl;

    // UART init
    int uart_fd = open_uart("/dev/ttyUSB0", B300, true);
    if(uart_fd < 0) return -1;

    // Init handshake
    const char initMsg[] = "/?!\r\n";
    write(uart_fd, initMsg, sizeof(initMsg)-1);

    char buf[1024];
    std::string rx;
    time_t last_time = time(nullptr);

    while(true) {
        // Lees data
        int len = read(uart_fd, buf, sizeof(buf)-1);
        if(len > 0) {
            buf[len] = '\0';
            rx += buf;
        }

        // Hebben we een compleet telegram?
        if(rx.find("!") != std::string::npos) {
            std::cout << "Ontvangen telegram:\n" << rx << std::endl;

            // Parse waarden
            extractValue(rx, "1.8.0", iskra162.totaal);
            extractValue(rx, "1.8.1", iskra162.laag);
            extractValue(rx, "1.8.2", iskra162.hoog);

            // Verbruik berekenen
            time_t now = time(nullptr);
            double seconds = difftime(now, last_time);
            if(seconds > 0) {
                iskra162.verbruik_W = ((iskra162.totaal - iskra162.totaal_oud) * 1000.0) / (seconds / 3600.0);
                iskra162.verbruik_A = static_cast<int>(iskra162.verbruik_W / 230.0);
                iskra162.totaal_oud = iskra162.totaal;
                last_time = now;
            }

            // Publiceer waarden
            auto publish = [&](const std::string &topic, const std::string &payload){
                pubmsg.payload = (void*)payload.c_str();
                pubmsg.payloadlen = payload.size();
                pubmsg.qos = QOS;
                pubmsg.retained = 0;
                MQTTClient_publishMessage(client, topic.c_str(), &pubmsg, &token);
                MQTTClient_waitForCompletion(client, token, TIMEOUT);
            };

            publish(TOPIC_TOTAAL, std::to_string(iskra162.totaal));
            publish(TOPIC_HOOG,   std::to_string(iskra162.hoog));
            publish(TOPIC_LAAG,   std::to_string(iskra162.laag));
            publish(TOPIC_WH,     std::to_string((int)iskra162.verbruik_W));
            publish(TOPIC_A,      std::to_string(iskra162.verbruik_A));

            std::cout << "Gegevens gepubliceerd naar MQTT." << std::endl;

            rx.clear(); // klaar voor volgende telegram
        }

        usleep(100000); // 100ms wachten
    }

    // Cleanup
    close(uart_fd);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
