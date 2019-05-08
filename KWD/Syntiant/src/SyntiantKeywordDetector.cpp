/*
 * Copyright 2017-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <memory>

#include <AVSCommon/Utils/Logger/Logger.h>

#include "Syntiant/SyntiantKeywordDetector.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace alexaClientSDK {
namespace kwd {

using namespace avsCommon::utils::logger;

/// String to identify log entries originating from this file.
static const std::string TAG("SyntiantKeywordDetector");

/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
#define LX(event) alexaClientSDK::avsCommon::utils::logger::LogEntry(TAG, event)

/// Timeout to use for read calls to the SharedDataStream.
const std::chrono::milliseconds TIMEOUT_FOR_READ_CALLS = std::chrono::milliseconds(1000);

std::unique_ptr<SyntiantKeywordDetector> SyntiantKeywordDetector::create(
    std::shared_ptr<avsCommon::avs::AudioInputStream> stream,
    avsCommon::utils::AudioFormat audioFormat,
    std::unordered_set<std::shared_ptr<avsCommon::sdkInterfaces::KeyWordObserverInterface>> keyWordObservers,
    std::unordered_set<std::shared_ptr<avsCommon::sdkInterfaces::KeyWordDetectorStateObserverInterface>>
    keyWordDetectorStateObservers,
    const std::string& kwdPipe) {
    if (!stream) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullStream"));
        return nullptr;
    }

    ACSDK_INFO(LX("Syntiant KWD Detector -- create"));

    std::unique_ptr<SyntiantKeywordDetector> detector(new SyntiantKeywordDetector(
        stream, keyWordObservers, keyWordDetectorStateObservers, audioFormat));

    if (!detector->init(kwdPipe)) {
	ACSDK_ERROR(LX("createFailed").d("reason", "initDetectorFailed"));
        return nullptr;
    }

    return detector;
}

SyntiantKeywordDetector::~SyntiantKeywordDetector() {
    m_isShuttingDown = true;
    if (m_detectionThread.joinable()) {
        m_detectionThread.join();
    }
}

SyntiantKeywordDetector::SyntiantKeywordDetector(
    std::shared_ptr<AudioInputStream> stream,
    std::unordered_set<std::shared_ptr<KeyWordObserverInterface>> keyWordObservers,
    std::unordered_set<std::shared_ptr<KeyWordDetectorStateObserverInterface>> keyWordDetectorStateObservers,
    avsCommon::utils::AudioFormat audioFormat) :
        AbstractKeywordDetector(keyWordObservers, keyWordDetectorStateObservers),
        m_stream{stream} {
}

bool SyntiantKeywordDetector::init(const std::string& kwdPipe)
{
    m_streamReader = m_stream->createReader(AudioInputStream::Reader::Policy::BLOCKING);
    if (!m_streamReader) {
        ACSDK_ERROR(LX("initFailed").d("reason", "createStreamReaderFailed"));
        return false;
    }

    m_kwdPipe = std::string(kwdPipe);
    if (!m_kwdPipe.length()) {
        m_kwdPipe = std::string("/home/pi/ndp-kwd");
    }

    m_isShuttingDown = false;
    m_detectionThread = std::thread(&SyntiantKeywordDetector::detectionLoop, this);
    return true;
}

void SyntiantKeywordDetector::detectionLoop() {
    m_beginIndexOfStreamReader = m_streamReader->tell();
    notifyKeyWordDetectorStateObservers(KeyWordDetectorStateObserverInterface::KeyWordDetectorState::ACTIVE);
    std::vector<int16_t> audioDataToPush(384*100);
    ssize_t wordsRead;

    char buf[100];
    int f;

    ACSDK_INFO(LX("Starting Syntiant detection loop"));


    f = open(m_kwdPipe.c_str(), O_RDONLY);

    while (!m_isShuttingDown) {
	bool didErrorOccur = false;
	wordsRead = readFromStream(m_streamReader,
				   m_stream,
				   audioDataToPush.data(),
				   audioDataToPush.size(),
				   TIMEOUT_FOR_READ_CALLS,
				   &didErrorOccur);
	(void) didErrorOccur;
	(void) wordsRead;
	int n;
	ioctl(f, FIONREAD, &n);
	if (n > 0) {
	    n = read(f, buf, strlen("alexa\n"));
	    //printf("[%llu] Read %d bytes: %s \n", m_streamReader->tell(), n, buf));
	    ACSDK_INFO(LX("Keyword detected"));
	    //printf("[%llu] Read %d bytes: %s \n", m_streamReader->tell(), n, buf);
	    notifyKeyWordObservers(m_stream,
				   "alexa",
				   KeyWordObserverInterface::UNSPECIFIED_INDEX,
		 		   m_streamReader->tell());

	}
    }

    ACSDK_INFO(LX("Stopping Syntiant detection loop\n"));

    m_streamReader->close();
}

}  // namespace kwd
}  // namespace alexaClientSDK
