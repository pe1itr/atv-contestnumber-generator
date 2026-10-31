#pragma once
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <glib.h>
#include <algorithm>
#include <string>
#include <vector>
struct LocalAddress { std::string interface, address; };
inline std::vector<LocalAddress> local_addresses(){
    std::vector<LocalAddress> result;struct ifaddrs *list=nullptr;
    if(getifaddrs(&list)!=0)return result;
    for(auto p=list;p;p=p->ifa_next){
        if(!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET||!(p->ifa_flags&IFF_UP)||(p->ifa_flags&IFF_LOOPBACK))continue;
        char ip[INET_ADDRSTRLEN];auto addr=reinterpret_cast<sockaddr_in*>(p->ifa_addr);
        if(inet_ntop(AF_INET,&addr->sin_addr,ip,sizeof(ip)))result.push_back({p->ifa_name,ip});
    }
    freeifaddrs(list);
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.interface<b.interface || (a.interface==b.interface&&a.address<b.address);});
    return result;
}
inline std::string phone_addresses(const std::string &uri,const std::vector<LocalAddress> &addresses){
    GUri *u=g_uri_parse(uri.c_str(),G_URI_FLAGS_NONE,nullptr);
    int port=u?g_uri_get_port(u):-1;std::string host=u&&g_uri_get_host(u)?g_uri_get_host(u):"";
    const char *query=u?g_uri_get_query(u):nullptr;
    GHashTable *params=query?g_uri_parse_params(query,-1,"&",G_URI_PARAMS_NONE,nullptr):nullptr;
    const char *mode=params?static_cast<const char*>(g_hash_table_lookup(params,"mode")):nullptr;
    bool listener=mode?std::string(mode)=="listener":host.empty();
    std::string text="Laptop — lokale IPv4-adressen";
    if(addresses.empty())text+="\nGeen actief IPv4-adres gevonden. Verbind de laptop met wifi of ethernet.";
    bool matching=false;
    for(const auto &a:addresses){
        text+="\n"+a.interface+": "+a.address;
        if(listener&&port>0&&port<=65535&&(host.empty()||host=="0.0.0.0"||host==a.address)){
            text+="   srt://"+a.address+":"+std::to_string(port)+"?mode=caller";matching=true;
        }
    }
    if(matching)text+="\nTelefoon: Caller, bovenstaande poort; kies het adres van hetzelfde wifi-/hotspotnetwerk.";
    else if(!listener)text+="\nLaptop staat als Caller ingesteld. Voor zenden vanaf je telefoon: zet de laptop op Listener.";
    else if(!addresses.empty())text+="\nControleer het luisteradres en de poort; er is geen passend bereikbaar IPv4-luisteradres.";
    if(params)g_hash_table_unref(params);
    if(u)g_uri_unref(u);
    return text;
}

inline std::string rtsp_phone_addresses(const std::string &uri,const std::vector<LocalAddress> &addresses){
    GUri *u=g_uri_parse(uri.c_str(),G_URI_FLAGS_NONE,nullptr);
    const char *host=u?g_uri_get_host(u):nullptr,*path=u?g_uri_get_path(u):nullptr;
    const int port=u?g_uri_get_port(u):-1;
    std::string text="Laptop — lokale IPv4-adressen";bool matching=false;
    for(const auto &a:addresses){
        text+="\n"+a.interface+": "+a.address;
        if(host&&path&&port>0&&port<=65535&&(std::string(host)=="0.0.0.0"||host==a.address)){
            text+="   rtsp://"+a.address+":"+std::to_string(port)+path;matching=true;
        }
    }
    if(matching)text+="\nLarix: bovenstaande RTSP-URL, transport TCP, video H.264 en geluid AAC.";
    else text+="\nGeen passend lokaal adres; controleer wifi/ethernet en het RTSP-luisteradres.";
    if(u)g_uri_unref(u);
    return text;
}
