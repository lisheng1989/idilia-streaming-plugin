# idilia-streaming-plugin

Streaming plugin for <b>Janus WebRTC Gateway v0.1.1</b> based on orginal streaming plugin. This streaming plugin incorporates GStreamer Multimedia Framework at the backend. It supports all kinds of media input starting from local files and ending with rtsp streams.

## TODO

- RTCP

## Building instructions

Follow the instructions from https://github.com/meetecho/janus-gateway/blob/master/README.md and then:

    git clone https://github.com/MotorolaSolutions/idilia-streaming-plugin.git
    cd idilia-streaming-plugin
    sh autogen.sh
    ./configure
    sudo make install configs
