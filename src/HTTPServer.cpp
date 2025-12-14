#include "HTTPServer.h"
#include "PluginProcessor.h"

HttpServerThread::HttpServerThread(PluginProcessor& _pluginProc)  : 
   juce::Thread("HTTP Server Thread"), 
   pluginProc{_pluginProc}
{
    initAPI();
}


void HttpServerThread::initAPI()
{
    // svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    //     DBG("HttpServerThread::log " << req.method << " " << req.path 
    //               << " -> " << res.status);
    // });

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        DBG("Got binary file " << BinaryData::originalFilenames[i]);

    }

// this macro can be controlled
// from the CMakeLists.txt file - you can choose
// if you want to serve your web files from disk or memory 
// serving from disk means you can quickly edit and reload for testing
#ifdef LOCAL_WEBUI    
    ///** start of server from file system code 
    // this code can be used in the standalone app 
    // to serve files from its 'ui' subdirectory
    // for easy testing where you can edit the 
    std::string workingDir = getBinary().string();
    std::string uiDir = workingDir + "/../../../../src/ui/";
    std::cout << "HTTPServer::  TESTING UI MODE: Serving from " << uiDir << std::endl;
    auto ret = svr.set_mount_point("/", uiDir);
    if (!ret) {
        // The specified base directory doesn't exist...
        std::cout << "Warning: trying to serve from a folder that does not exist " << std::endl;
    }
      
    ///**  end of server from file system code 

#else
    std::cout << "HTTPServer:: serving from memory not disk " << std::endl;

    // Deploy version
    // route for the main index file
    /// *** start of serving files from the linked 'binary'
    svr.Get("/index.html", [](const httplib::Request& req, httplib::Response& res) {
        int size = 0;
        const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[0], size);
    
        if (data != nullptr) {
            res.set_content(data, static_cast<size_t>(size), "text/html");
        } else {
            res.status = 404;
            res.set_content("404: File not found", "text/plain");
        }
    });
    ///*** end of serving files from the linked 'binary'
#endif

    // 'live' responders for button presses - call to the pluginprocessor 
    svr.Get("/button1", [this](const httplib::Request& req, httplib::Response& res) {
        this->pluginProc.messageReceivedFromWebAPI("Button 1 clicked");
    });
    svr.Get("/button2", [this](const httplib::Request& req, httplib::Response& res) {
        this->pluginProc.messageReceivedFromWebAPI("Button 2 clicked");
    });
    svr.Post("/addSamplePlayer", [this](const httplib::Request& req, httplib::Response& res) {
        DBG("API: post /addSamplePlayer");
        this->pluginProc.addSamplePlayerFromWeb();
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    svr.Post("/loadSample", [this](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("id");
        if (it == req.params.end())
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"missing id\"}", "application/json");
            return;
        }

        try
        {
            int id = std::stoi(it->second);
            this->pluginProc.requestSampleLoadFromWeb (id);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (const std::exception&)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"invalid id\"}", "application/json");
        }
    });

    svr.Get("/state", [this](const httplib::Request& req, httplib::Response& res) {
        juce::ignoreUnused (req);
        auto state = pluginProc.getSamplerState();
        const auto json = juce::JSON::toString (state).toStdString();
        res.set_content (json, "application/json");
    });

    svr.Get("/waveform", [this](const httplib::Request& req, httplib::Response& res) {
        auto idIt = req.params.find ("id");
        if (idIt == req.params.end())
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"missing id\"}", "application/json");
            return;
        }

        try
        {
            int id = std::stoi (idIt->second);
            const auto svg = pluginProc.getWaveformSVGForPlayer (id);
            res.set_content (svg.toStdString(), "image/svg+xml");
        }
        catch (const std::exception&)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"invalid id\"}", "application/json");
        }
    });

    svr.Get("/vuState", [this](const httplib::Request& req, httplib::Response& res) {
        juce::ignoreUnused (req);
        const auto jsonStr = pluginProc.getVuStateJson();
        res.set_content (jsonStr, "application/json");
    });

    svr.Post("/setRange", [this](const httplib::Request& req, httplib::Response& res) {
        auto idIt = req.params.find("id");
        auto lowIt = req.params.find("low");
        auto highIt = req.params.find("high");

        if (idIt == req.params.end() || lowIt == req.params.end() || highIt == req.params.end())
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"missing parameters\"}", "application/json");
            return;
        }

        try
        {
            int id = std::stoi (idIt->second);
            int low = std::stoi (lowIt->second);
            int high = std::stoi (highIt->second);
            pluginProc.setSampleRangeFromWeb (id, low, high);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (const std::exception&)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"invalid parameters\"}", "application/json");
        }
    });

    svr.Post("/trigger", [this](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("id");
        if (it == req.params.end())
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"missing id\"}", "application/json");
            return;
        }

        try
        {
            int id = std::stoi (it->second);
            pluginProc.triggerFromWeb (id);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (const std::exception&)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"invalid id\"}", "application/json");
        }
    });


}


void HttpServerThread::run()
{

    DBG("API server starting");

    // Run the server in a blocking loop until stopThread() is called
    svr.listen("0.0.0.0", 8080);

}

void HttpServerThread::stopServer()
{
    DBG("API server shutting down");

    svr.stop();
    stopThread(1000); // Gracefully stop thread
}
    
