#include <iostream>
#include <unistd.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>

using std::cout;
using std::string;
using std::vector;

template <typename K, typename V>
class Dict {
    public:

    vector<K> Keys;
    vector<V> Values;


    void Set(K name, V value) {
        Keys.push_back(name);
        Values.push_back(value);
    }

    V* Get(K name) {
        for (int i = 0; i < Keys.size(); i++) {
            if (Keys[i] == name) {
                return &Values[i];
            }
        }

        return nullptr; // Maybe return something else here.
    }

    Dict() {
        Keys = vector<K>();
        Values = vector<V>();
    }
};

class Headers {
    public:

    vector<string> Keys;
    vector<string> Values;

    string Method;
    string Path;

    // Where the HTTP body starts
    int BodyStart;

    void Set(string name, string value) {
        Keys.push_back(name);
        Values.push_back(value);
    }

    string GetHTTPRepresentation(string name) {
        for (int i = 0; i <= Keys.size(); i++) {
            if (Keys[i] == name) {
                return name + ": " + Values[i] + "\n";
            }
        }

        return name + ": 0"; // Maybe return something else here.
    }

    string Get(string name) {
        for (int i = 0; i <= Keys.size(); i++) {
            if (Keys[i] == name) {
                return Values[i];
            }
        }

        return name + ": 0"; // Maybe return something else here.
    }

    Headers() {
        Keys = vector<string>();
        Values = vector<string>();
    }
};

class Request {
    int Socket;

    public:
    
    bool Responded;

    Headers HttpHeaders;
    Headers ResponseHeaders;

    string Body;

    void Respond(int status, string data) {
        std::ostringstream stream;

        stream << "HTTP/1.1 " << status << " OK\n";

        ResponseHeaders.Set("Content-Length", std::to_string(data.length()));

        for (int i = 0; i < ResponseHeaders.Keys.size(); i++) {
            stream << ResponseHeaders.Keys[i] << ": " << ResponseHeaders.Values[i] << "\n";
        }

        stream << "\n" << data;

        write(Socket, stream.str().c_str(), stream.str().length());

        close(Socket);

        Responded = true;
    }

    Request(int fd, Headers headers, string body) {
        Socket = fd;
        HttpHeaders = headers;
        ResponseHeaders = Headers();

        Responded = false;

        Body = body;
    }
};

typedef void (*HTTPCallback)(Request*);

class CGIContext {
    private:

    Request* Req;
    string Exe;

    void SetEnviron() {
        setenv("SERVER_SOFTWARE", "HTPP", 1);

        setenv("SERVER_NAME", "localhost", 1);

        setenv("GATEWAY_INTERFACE", "cgi/1.1", 1);

        setenv("SERVER_PROTOCOL", "http/1.1", 1);

        setenv("REQUEST_METHOD", Req->HttpHeaders.Method.c_str(), 1);

        setenv("SCRIPT_NAME", Exe.c_str(), 1);

        if (Req->HttpHeaders.Method == "POST") {
            setenv("CONTENT_TYPE", Req->HttpHeaders.Get("Content-Type").c_str(), 1);
            setenv("CONTENT_LENGTH", Req->HttpHeaders.Get("Content-Length").c_str(), 1);
        }
        else {
            unsetenv("CONTENT_TYPE");
            unsetenv("CONTENT_LENGTH");
        }
    }

    Dict<string, string> ParseHeaders(string result) {
        Dict<string, string> headers = Dict<string, string>();

        string name = "";
        string value = "";
        bool isname = true;

        for (int i = 0; i < result.length(); i++) {
            if (result[i] == ' ')
                continue;
            
            if (result[i] == '\n') {
                headers.Set(name, value);

                if (result[i + 1] == '\n') {
                    headers.Set("Output", result.substr(i + 2));

                    break;
                }
                
                name = "";
                value = "";

                isname = true;

                continue;
            }

            if (result[i] == ':') {
                isname = false;
                
                continue;
            }

            if (isname)
                name += result[i];
            else
                value += result[i];
        }

        return headers;
    }

    public:
    
    void Run() {
        // Make sure it is executable first.
        system(("chmod +x " + Exe).c_str());

        SetEnviron();

        FILE* prog = popen(("./" + Exe).c_str(), "r");

        string result = "";

        char buf[100];

        int read;

        while ((read = fread(buf, 1, 100, prog)) > 0) {
            for (int i = 0; i < read; i++) {
                result += buf[i];
            }
        }

        Dict<string, string> headers = ParseHeaders(result);

        if (headers.Get("Content-type") != nullptr)
            Req->ResponseHeaders.Set("Content-Type", *headers.Get("Content-type"));

        for (int i = 0; i < headers.Keys.size(); i++) {
            if (headers.Keys[i] != "Content-type" && headers.Keys[i] != "Status" && headers.Keys[i] != "Output")
                Req->ResponseHeaders.Set(headers.Keys[i], headers.Values[i]);
        }

        string* statstr = headers.Get("Status");

        int status = statstr != nullptr ? stoi(statstr->substr(0, 3)) : 200;

        Req->Respond(status, *headers.Get("Output"));

        pclose(prog);
    }

    CGIContext(Request* req, string exe) {
        Req = req;
        Exe = exe;
    }
};

class Server {
    private:

    std::thread Loop;
    Dict<string, HTTPCallback> Handlers;

    string CGIDir;

    Dict<string, string> CGIs;

    Headers ParseHeaders(string data) {
        Headers headers = Headers();
        string curname = "";
        string curvalue = "";
        bool name = true;

        headers.Method = "";
        headers.Path = "";
        bool method = true;

        int headerloc;

        // Read request info (Method, Path, HTTP Version)
        for (int i = 0; i < data.length(); i++) {
            if (data[i] == ' ') {
                if (! method) {
                    headerloc = i + 1;
                    break;
                }

                method = false;
                continue;
            }

            if (method) {
                headers.Method += data[i];
            }
            else {
                headers.Path += data[i];
            }
        }

        for (int i = headerloc; i < data.length(); i++) {
            if (data[i] == '\n') {
                name = true;
                headers.Set(curname, curvalue);
                curname = "";
                curvalue = "";

                headers.BodyStart = i + 1;

                if (data[i + 1] == '\n')
                    break;

                continue;
            }

            if (data[i] == ':') {
                name = false;
                continue;
            }

            if (name)
                curname += data[i];
            else
                curvalue += data[i];
        }

        return headers;
    }

    public:
    bool Running;
    
    void Get(string path, HTTPCallback callback) {
        Handlers.Set("GET" + path, callback);
    }

    void Post(string path, HTTPCallback callback) {
        Handlers.Set("POST" + path, callback);
    }

    void Start(int port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0) {
            std::cerr << "Unable to open socket.\n";
            return;
        }

        sockaddr_in addr;

        in_addr inaddr;

        inaddr.s_addr = INADDR_ANY;

        addr.sin_family = AF_INET;
        addr.sin_addr = inaddr;
        addr.sin_port = htons(8080);

        int num = 1;

        // TCP sockets go into a TIME_WAIT state where they cant be used for about a minute.
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &num, sizeof(int));

        if (bind(fd, (const sockaddr*) &addr, sizeof(sockaddr_in)) < 0) {
            std::cerr << "Socket failed to bind.\n";
            return;
        };

        listen(fd, 20);

        Running = true;

        Loop = std::thread([this, fd] () {
            char buf[1024];

            while (Running) {
                string body = "";
                
                sockaddr* clientaddr = (sockaddr*) malloc(sizeof(sockaddr));
                socklen_t* len = (socklen_t*) malloc(sizeof(socklen_t));

                int req = accept(fd, clientaddr, len);

                // Free unused objects
                free(clientaddr);
                free(len);

                int count = read(req, buf, 1024);

                if (count == 1024) {
                    while (count == 1024) {
                        body += buf;
                        count = read(req, buf, 1024);
                    }
                }
                else {
                    body += buf;
                }

                Headers info = ParseHeaders(body);
                
                HTTPCallback* callback = Handlers.Get(info.Method + info.Path);

                // HTTP Request object is created here.
                Request* httpreq = new Request(req, info, info.Method != "GET" ? body.substr(info.BodyStart, stoi(info.Get("Content-Length"))) : "");

                if (CGIs.Get(info.Path) != nullptr) {
                    CGIContext context = CGIContext(httpreq, *CGIs.Get(info.Path));

                    context.Run();

                    continue;
                }

                if (callback == nullptr) {
                    HTTPCallback* notfound = Handlers.Get("GET404");

                    if (notfound != nullptr) {
                        (*notfound)(httpreq);

                        if (! httpreq->Responded) {
                            std::cerr << "HTTP Handlers must call the respond method!\n\n";
                            
                            throw std::exception();
                        }
                    }
                    else {
                        httpreq->Respond(404, "Not Found");
                    }
                    
                    continue;
                }
                
                (*callback)(httpreq);

                if (! httpreq->Responded) {
                    std::cerr << "HTTP Handlers must call the respond method!\n\n";

                    throw std::exception();
                }
            }
            
            close(fd);
        });
    }

    void SetCGI(string path, string exe) {
        CGIs.Set(path, exe);
    }

    void SetCGIDir(string path, string dir) {
        CGIDir = dir;
        DIR* cgi = opendir(dir.c_str());
        readdir(cgi);
        readdir(cgi); // Get rid of . and ..

        dirent* entry = readdir(cgi);

        while (entry != nullptr) {
            SetCGI(path + "/" + entry->d_name, dir + "/" + entry->d_name);

            entry = readdir(cgi);
        }

        closedir(cgi);
    }

    Server() {
        Running = false;
    }
};