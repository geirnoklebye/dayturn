/** 
 * @file streamingaudio_fmodstudio.cpp
 * @brief LLStreamingAudio_FMODSTUDIO implementation
 *
 * $LicenseInfo:firstyear=2020&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2020, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llmath.h"

#include "fmodstudio/fmod.hpp"
#include "fmodstudio/fmod_errors.h"

#include "llstreamingaudio_fmodstudio.h"


class LLAudioStreamManagerFMODSTUDIO
{
public:
    LLAudioStreamManagerFMODSTUDIO(FMOD::System *system, const std::string& url);
    FMOD::Channel* startStream();
    bool stopStream(); // Returns true if the stream was successfully stopped.
    bool ready();

    const std::string& getURL() 	{ return mInternetStreamURL; }

    FMOD_OPENSTATE getOpenState(unsigned int* percentbuffered = nullptr, bool* starving = nullptr, bool* diskbusy = nullptr);
protected:
    FMOD::System* mSystem;
    FMOD::Channel* mStreamChannel;
    FMOD::Sound* mInternetStream;
    bool mReady;

    std::string mInternetStreamURL;
};



//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
LLStreamingAudio_FMODSTUDIO::LLStreamingAudio_FMODSTUDIO(FMOD::System *system) :
mSystem(system),
mCurrentInternetStreamp(nullptr),
mFMODInternetStreamChannelp(nullptr),
mGain(1.0f),
mRetryCount(0)
{
    // Number of milliseconds of audio to buffer for the audio card.
    // Must be larger than the usual Second Life frame stutter time.
    const U32 buffer_seconds = 10;		//sec
    const U32 estimated_bitrate = 128;	//kbit/sec
    FMOD_RESULT result = mSystem->setStreamBufferSize(estimated_bitrate * buffer_seconds * 128/*bytes/kbit*/, FMOD_TIMEUNIT_RAWBYTES);
    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << "setStreamBufferSize error: " << FMOD_ErrorString(result) << LL_ENDL;
    }

    // Here's where we set the size of the network buffer and some buffering 
    // parameters.  In this case we want a network buffer of 16k, we want it 
    // to prebuffer 40% of that when we first connect, and we want it 
    // to rebuffer 80% of that whenever we encounter a buffer underrun.

    // Leave the net buffer properties at the default.
    //FSOUND_Stream_Net_SetBufferProperties(20000, 40, 80);
}


LLStreamingAudio_FMODSTUDIO::~LLStreamingAudio_FMODSTUDIO()
{
    if (mCurrentInternetStreamp)
    {
        // Isn't supposed to hapen, stream should be clear by now,
        // and if it does, we are likely going to crash.
        LL_WARNS("FMOD") << "mCurrentInternetStreamp not null on shutdown!" << LL_ENDL;
        stop();
    }

    // Kill dead internet streams, if possible
    killDeadStreams();

    if (!mDeadStreams.empty())
    {
        // LLStreamingAudio_FMODSTUDIO was inited on startup
        // and should be destroyed on shutdown, it should
        // wait for streams to die to not cause crashes or
        // leaks.
        // Ideally we need to wait on some kind of callback
        // to release() streams correctly, but 200 ms should
        // be enough and we can't wait forever.
        LL_INFOS("FMOD") << "Waiting for " << (S32)mDeadStreams.size() << " streams to stop" << LL_ENDL;
        for (S32 i = 0; i < 20; i++)
        {
            const U32 ms_delay = 10;
            ms_sleep(ms_delay); // rude, but not many options here
            killDeadStreams();
            if (mDeadStreams.empty())
            {
                LL_INFOS("FMOD") << "All streams stopped after " << (S32)((i + 1) * ms_delay) << "ms" << LL_ENDL;
                break;
            }
        }
    }

    if (!mDeadStreams.empty())
    {
        LL_WARNS("FMOD") << "Failed to kill some audio streams" << LL_ENDL;
    }
}

void LLStreamingAudio_FMODSTUDIO::killDeadStreams()
{
    std::list<LLAudioStreamManagerFMODSTUDIO *>::iterator iter;
    for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
    {
        LLAudioStreamManagerFMODSTUDIO *streamp = *iter;
        if (streamp->stopStream())
        {
            LL_INFOS("FMOD") << "Closed dead stream" << LL_ENDL;
            delete streamp;
            mDeadStreams.erase(iter++);
        }
        else
        {
            iter++;
        }
    }
}

void LLStreamingAudio_FMODSTUDIO::start(const std::string& url)
{
    //if (!mInited)
    //{
    //	LL_WARNS() << "startInternetStream before audio initialized" << LL_ENDL;
    //	return;
    //}

    // "stop" stream but don't clear url, etc. in case url == mInternetStreamURL
    stop();

    if (!url.empty())
    {
        LL_INFOS("FMOD") << "Starting internet stream: " << url << LL_ENDL;
        mCurrentInternetStreamp = new LLAudioStreamManagerFMODSTUDIO(mSystem, url);
        mURL = url;
    }
    else
    {
        LL_INFOS("FMOD") << "Set internet stream to null" << LL_ENDL;
        mURL.clear();
    }

    mRetryCount = 0;
}


void LLStreamingAudio_FMODSTUDIO::update()
{
    // Kill dead internet streams, if possible
    killDeadStreams();

    // Don't do anything if there are no streams playing
    if (!mCurrentInternetStreamp)
    {
        return;
    }

    unsigned int progress;
    bool starving;
    bool diskbusy;
    FMOD_OPENSTATE open_state = mCurrentInternetStreamp->getOpenState(&progress, &starving, &diskbusy);

    if (open_state == FMOD_OPENSTATE_READY)
    {
        // Stream is live

        // start the stream if it's ready
        if (!mFMODInternetStreamChannelp &&
            (mFMODInternetStreamChannelp = mCurrentInternetStreamp->startStream()))
        {
            // Reset volume to previously set volume
            setGain(getGain());
            mFMODInternetStreamChannelp->setPaused(false);
        }
        mRetryCount = 0;
    }
    else if (open_state == FMOD_OPENSTATE_ERROR)
    {
        LL_INFOS("FMOD") << "State: FMOD_OPENSTATE_ERROR"
            << " Progress: " << U32(progress)
            << " Starving: " << S32(starving)
            << " Diskbusy: " << S32(diskbusy) << LL_ENDL;
        if (mRetryCount < 2)
        {
            // Retry
            std::string url = mURL;
            stop(); // might drop mURL, drops mCurrentInternetStreamp

            mRetryCount++;

            if (!url.empty())
            {
                LL_INFOS("FMOD") << "Restarting internet stream: " << url  << ", attempt " << (mRetryCount + 1) << LL_ENDL;
                mCurrentInternetStreamp = new LLAudioStreamManagerFMODSTUDIO(mSystem, url);
                mURL = url;
            }
        }
        else
        {
            stop();
        }
        return;
    }

    if (mFMODInternetStreamChannelp)
    {
        FMOD::Sound *sound = nullptr;

        if (mFMODInternetStreamChannelp->getCurrentSound(&sound) == FMOD_OK && sound)
        {
            FMOD_TAG tag;
            S32 tagcount, dirtytagcount;

            if (sound->getNumTags(&tagcount, &dirtytagcount) == FMOD_OK && dirtytagcount)
            {
				// <FS:CR> Stream metadata - originally by Shyotl Khur
				mMetadata.clear();
				mNewMetadata = true;
				// </FS:CR>
                for (S32 i = 0; i < tagcount; ++i)
                {
                    if (sound->getTag(nullptr, i, &tag) != FMOD_OK)
                        continue;

					std::string name = tag.name;
					switch(tag.type)
					{
						case(FMOD_TAGTYPE_ID3V2):
						{
							if(name == "TIT2") name = "TITLE";
							else if(name == "TPE1") name = "ARTIST";
							break;
						}
						case(FMOD_TAGTYPE_ASF):
						{
							if(name == "Title") name = "TITLE";
							else if(name == "WM/AlbumArtist") name = "ARTIST";
							break;
						}
						case(FMOD_TAGTYPE_FMOD):
						{
							if (!strcmp(tag.name, "Sample Rate Change"))
							{
                            LL_INFOS("FMOD") << "Stream forced changing sample rate to " << *((float *)tag.data) << LL_ENDL;
								mFMODInternetStreamChannelp->setFrequency(*((float *)tag.data));
							}
							continue;
						}
						default:
					    	{
							if(name == "icy-name") {
								name = "STREAM_NAME";
							}
							else if(name == "icy-url") {
								name = "STREAM_LOCATION";
							}
							break;
						}
					}
					switch(tag.datatype)
					{
						case(FMOD_TAGDATATYPE_INT):
						{
							(mMetadata)[name]=*(LLSD::Integer*)(tag.data);
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << *(int*)(tag.data) << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_FLOAT):
						{
							(mMetadata)[name]=*(LLSD::Real*)(tag.data);
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << *(float*)(tag.data) << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING):
						{
							std::string out = rawstr_to_utf8(std::string((char*)tag.data,tag.datalen));
							(mMetadata)[name]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING_UTF16):
						{
							std::string out((char*)tag.data,tag.datalen);
							(mMetadata)[std::string(tag.name)]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING_UTF16BE):
						{
							std::string out((char*)tag.data,tag.datalen);
							//U16* buf = (U16*)out.c_str();
							//for(U32 j = 0; j < out.size()/2; ++j)
								//(((buf[j] & 0xff)<<8) | ((buf[j] & 0xff00)>>8));
							(mMetadata)[std::string(tag.name)]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						default:
							break;
					}
					// </FS:CR> Stream metadata - originally by Shyotl Khur
                }
            }

            if (starving)
            {
                bool paused = false;
                mFMODInternetStreamChannelp->getPaused(&paused);
                if (!paused)
                {
                    LL_INFOS("FMOD") << "Stream starvation detected! Pausing stream until buffer nearly full." << LL_ENDL;
                    LL_INFOS("FMOD") << "  (diskbusy=" << diskbusy << ")" << LL_ENDL;
                    LL_INFOS("FMOD") << "  (progress=" << progress << ")" << LL_ENDL;
                    mFMODInternetStreamChannelp->setPaused(true);
                }
            }
            else if (progress > 80)
            {
                mFMODInternetStreamChannelp->setPaused(false);
            }
        }
    }
}

void LLStreamingAudio_FMODSTUDIO::stop()
{
    if (mFMODInternetStreamChannelp)
    {
        mFMODInternetStreamChannelp->setPaused(true);
        mFMODInternetStreamChannelp->setPriority(0);
        mFMODInternetStreamChannelp = nullptr;
    }

    if (mCurrentInternetStreamp)
    {
        LL_INFOS("FMOD") << "Stopping internet stream: " << mCurrentInternetStreamp->getURL() << LL_ENDL;
        if (mCurrentInternetStreamp->stopStream())
        {
            delete mCurrentInternetStreamp;
        }
        else
        {
            LL_WARNS("FMOD") << "Pushing stream to dead list: " << mCurrentInternetStreamp->getURL() << LL_ENDL;
            mDeadStreams.push_back(mCurrentInternetStreamp);
        }
        mCurrentInternetStreamp = nullptr;
        //mURL.clear();
    }
}

void LLStreamingAudio_FMODSTUDIO::pause(int pauseopt)
{
    if (pauseopt < 0)
    {
        pauseopt = mCurrentInternetStreamp ? 1 : 0;
    }

    if (pauseopt)
    {
        if (mCurrentInternetStreamp)
        {
            LL_INFOS("FMOD") << "Pausing internet stream" << LL_ENDL;
            stop();
        }
    }
    else
    {
        start(getURL());
    }
}


// A stream is "playing" if it has been requested to start.  That
// doesn't necessarily mean audio is coming out of the speakers.
int LLStreamingAudio_FMODSTUDIO::isPlaying()
{
    if (mCurrentInternetStreamp)
    {
        return 1; // Active and playing
    }
    else if (!mURL.empty())
    {
        return 2; // "Paused"
    }
    else
    {
        return 0;
    }
}


F32 LLStreamingAudio_FMODSTUDIO::getGain()
{
    return mGain;
}


std::string LLStreamingAudio_FMODSTUDIO::getURL()
{
    return mURL;
}


void LLStreamingAudio_FMODSTUDIO::setGain(F32 vol)
{
    mGain = vol;

    if (mFMODInternetStreamChannelp)
    {
        vol = llclamp(vol * vol, 0.f, 1.f);	//should vol be squared here?

        mFMODInternetStreamChannelp->setVolume(vol);
    }
}

bool LLStreamingAudio_FMODSTUDIO::hasNewMetadata()
{
	if (mCurrentInternetStreamp && mNewMetadata) {
		mNewMetadata = false;
		return true;
	}

	return false;
}

std::string LLStreamingAudio_FMODSTUDIO::getCurrentArtist()
{
	return mMetadata["ARTIST"].asString();
}

std::string LLStreamingAudio_FMODSTUDIO::getCurrentTitle()
{
	return mMetadata["TITLE"].asString();
}

std::string LLStreamingAudio_FMODSTUDIO::getCurrentStreamName()
{
	return mMetadata["STREAM_NAME"].asString();
}

std::string LLStreamingAudio_FMODSTUDIO::getCurrentStreamLocation()
{
	return mMetadata["STREAM_LOCATION"].asString();
}

///////////////////////////////////////////////////////
// manager of possibly-multiple internet audio streams

LLAudioStreamManagerFMODSTUDIO::LLAudioStreamManagerFMODSTUDIO(FMOD::System *system, const std::string& url) :
mSystem(system),
mStreamChannel(nullptr),
mInternetStream(nullptr),
mReady(false)
{
    mInternetStreamURL = url;

    FMOD_RESULT result = mSystem->createStream(url.c_str(), FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS, 0, &mInternetStream);

    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << "Couldn't open fmod stream, error "
            << FMOD_ErrorString(result)
            << LL_ENDL;
        mReady = false;
        return;
    }

    mReady = true;
}

FMOD::Channel *LLAudioStreamManagerFMODSTUDIO::startStream()
{
    // We need a live and opened stream before we try and play it.
    if (!mInternetStream || getOpenState() != FMOD_OPENSTATE_READY)
    {
        LL_WARNS("FMOD") << "No internet stream to start playing!" << LL_ENDL;
        return nullptr;
    }

    if (mStreamChannel)
        return mStreamChannel;	//Already have a channel for this stream.

    FMOD_RESULT result = mSystem->playSound(mInternetStream, nullptr, true, &mStreamChannel);
    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << FMOD_ErrorString(result) << LL_ENDL;
    }
    return mStreamChannel;
}

bool LLAudioStreamManagerFMODSTUDIO::stopStream()
{
    if (mInternetStream)
    {


        bool close = true;
        switch (getOpenState())
        {
        case FMOD_OPENSTATE_CONNECTING:
            close = false;
            break;
        default:
            close = true;
        }

        if (close)
        {
            mInternetStream->release();
            mStreamChannel = nullptr;
            mInternetStream = nullptr;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return true;
    }
}

FMOD_OPENSTATE LLAudioStreamManagerFMODSTUDIO::getOpenState(unsigned int* percentbuffered, bool* starving, bool* diskbusy)
{
    FMOD_OPENSTATE state;
    FMOD_RESULT result = mInternetStream->getOpenState(&state, percentbuffered, starving, diskbusy);
    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << FMOD_ErrorString(result) << LL_ENDL;
    }
    return state;
}

void LLStreamingAudio_FMODSTUDIO::setBufferSizes(U32 streambuffertime, U32 decodebuffertime)
{
    FMOD_RESULT result = mSystem->setStreamBufferSize(streambuffertime / 1000 * 128 * 128, FMOD_TIMEUNIT_RAWBYTES);
    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << "setStreamBufferSize error: " << FMOD_ErrorString(result) << LL_ENDL;
        return;
    }
    FMOD_ADVANCEDSETTINGS settings;
    memset(&settings, 0, sizeof(settings));
    settings.cbSize = sizeof(settings);
    settings.defaultDecodeBufferSize = decodebuffertime;//ms
    result = mSystem->setAdvancedSettings(&settings);
    if (result != FMOD_OK)
    {
        LL_WARNS("FMOD") << "setAdvancedSettings error: " << FMOD_ErrorString(result) << LL_ENDL;
    }
}
