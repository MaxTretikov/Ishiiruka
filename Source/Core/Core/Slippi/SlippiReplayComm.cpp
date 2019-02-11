#include "SlippiReplayComm.h"
#include "Common/FileUtil.h"

#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"

// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
static inline void ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s)
{
	ltrim(s);
	rtrim(s);
}

SlippiReplayComm::SlippiReplayComm()
{
	INFO_LOG(EXPANSIONINTERFACE, "SlippiReplayComm: Using playback config path: %s",
	         SConfig::GetInstance().m_strSlippiInput.c_str());
	configFilePath = SConfig::GetInstance().m_strSlippiInput.c_str();
}

SlippiReplayComm::~SlippiReplayComm() {}

SlippiReplayComm::CommSettings SlippiReplayComm::getSettings()
{
	return commFileSettings;
}

bool SlippiReplayComm::isNewReplay()
{
	loadFile();
	std::string replayFilePath = commFileSettings.replayPath;

	// TODO: This logic for detecting a new replay isn't quite good enough
	// TODO: wont work in the case where someone tries to load the same
	// TODO: replay twice in a row
	bool isNewReplay = replayFilePath != previousReplayLoaded;
	bool isReplay = !!replayFilePath.length();

	return isReplay && isNewReplay;
}

Slippi::SlippiGame *SlippiReplayComm::loadGame()
{
	auto replayFilePath = commFileSettings.replayPath;
	INFO_LOG(EXPANSIONINTERFACE, "Attempting to load replay file %s", replayFilePath.c_str());
	auto result = Slippi::SlippiGame::FromFile(replayFilePath);
	if (result)
	{
		// If we successfully loaded a SlippiGame, indicate as such so 
		// that this game won't be considered new anymore. If the replay
		// file did not exist yet, result will be falsy, which will keep
		// the replay considered new so that the file will attempt to be
		// loaded again
		previousReplayLoaded = replayFilePath;
	}

	return result;
}

void SlippiReplayComm::loadFile()
{
	// TODO: Maybe load file in a more intelligent way to save
	// TODO: file operations
	std::string commFileContents;
	File::ReadFileToString(configFilePath, commFileContents);

	auto res = json::parse(commFileContents, nullptr, false);
	if (res.is_discarded() || !res.is_object())
	{
		// Happens if there is a parse error, I think?
		commFileSettings.replayPath = "";
		commFileSettings.isRealTimeMode = false;

		if (res.is_string())
		{
			// If we have a string, let's use that as the replayPath
			// This is really only here because when developing it might be easier
			// to just throw in a string instead of an object
			commFileSettings.replayPath = res;
		}	
		
		return;
	}

	// TODO: Support file with only path string
	commFileSettings.replayPath = res.value("replay", "");
	commFileSettings.isRealTimeMode = res.value("isRealTimeMode", false);
}
