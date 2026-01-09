#pragma once
#include <string>

struct NetConfig {
    bool use_dhcp = true;
    std::string static_ip = "192.168.1.50";
    std::string gateway   = "192.168.1.1";
    std::string netmask   = "255.255.255.0";
    std::string dns       = "8.8.8.8";
};
