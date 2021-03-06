#include "base/logging.h"
#include "i18n/i18n.h"
#include "file/ini_file.h"
#include "file/vfs.h"

I18NRepo i18nrepo;

I18NRepo::~I18NRepo() {
	Clear();
}

void I18NRepo::Clear() {
	for (auto iter = cats_.begin(); iter != cats_.end(); ++iter) {
		delete iter->second;
	}
	cats_.clear();
}

const char *I18NCategory::T(const char *key, const char *def) {
	if (!key) {
		return "ERROR";
	}
	// Replace the \n's with \\n's so that key values with newlines will be found correctly.
	std::string modifiedKey = key;
	modifiedKey = ReplaceAll(modifiedKey, "\n", "\\n");

	auto iter = map_.find(modifiedKey);
	if (iter != map_.end()) {
//		ILOG("translation key found in %s: %s", name_.c_str(), key);
		return iter->second.text.c_str();
	} else {
		if (def)
			missedKeyLog_[key] = def;
		else
			missedKeyLog_[key] = modifiedKey.c_str();
//		ILOG("Missed translation key in %s: %s", name_.c_str(), key);
		return def ? def : key;
	}
}

void I18NCategory::SetMap(const std::map<std::string, std::string> &m) {
	for (auto iter = m.begin(); iter != m.end(); ++iter) {
		if (map_.find(iter->first) == map_.end()) {
			std::string text = ReplaceAll(iter->second, "\\n", "\n");
			map_[iter->first] = I18NEntry(text);
//			ILOG("Language entry: %s -> %s", iter->first.c_str(), text.c_str());
		}
	}
}

I18NCategory *I18NRepo::GetCategory(const char *category) {
	auto iter = cats_.find(category);
	if (iter != cats_.end()) {
		return iter->second;
	} else {
		I18NCategory *c = new I18NCategory(this, category);
		cats_[category] = c;
		return c;
	}
}

std::string I18NRepo::GetIniPath(const std::string &languageID) const {
	return "lang/" + languageID + ".ini";
}

bool I18NRepo::IniExists(const std::string &languageID) const {
	FileInfo info;
	if (!VFSGetFileInfo(GetIniPath(languageID).c_str(), &info))
		return false;
	if (!info.exists)
		return false;
	return true;
}

bool I18NRepo::LoadIni(const std::string &languageID, const std::string &overridePath) {
	IniFile ini;
	std::string iniPath;

//	ILOG("Loading lang ini %s", iniPath.c_str());
	if (!overridePath.empty()) {
		iniPath = overridePath + languageID + ".ini";
	} else {
		iniPath = GetIniPath(languageID);
	}

	if (!ini.LoadFromVFS(iniPath))
		return false;

	Clear();

	const std::vector<IniFile::Section> &sections = ini.Sections();

	for (auto iter = sections.begin(); iter != sections.end(); ++iter) {
		if (iter->name() != "") {
			cats_[iter->name()] = LoadSection(&(*iter), iter->name().c_str());
		}
	}
	return true;
}

I18NCategory *I18NRepo::LoadSection(const IniFile::Section *section, const char *name) {
	I18NCategory *cat = new I18NCategory(this, name);
	std::map<std::string, std::string> sectionMap = section->ToMap();
	cat->SetMap(sectionMap);
	return cat;
}

// This is a very light touched save variant - it won't overwrite 
// anything, only create new entries.
void I18NRepo::SaveIni(const std::string &languageID) {
	IniFile ini;
	ini.Load(GetIniPath(languageID));
	for (auto iter = cats_.begin(); iter != cats_.end(); ++iter) {
		std::string categoryName = iter->first;
		IniFile::Section *section = ini.GetOrCreateSection(categoryName.c_str());
		SaveSection(ini, section, iter->second);
	}
	ini.Save(GetIniPath(languageID));
}

void I18NRepo::SaveSection(IniFile &ini, IniFile::Section *section, I18NCategory *cat) {
	const std::map<std::string, std::string> &missed = cat->Missed();

	for (auto iter = missed.begin(); iter != missed.end(); ++iter) {
		if (!section->Exists(iter->first.c_str())) {
			std::string text = ReplaceAll(iter->second, "\n", "\\n");
			section->Set(iter->first, text);
		}
	}

	const std::map<std::string, I18NEntry> &entries = cat->GetMap();
	for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
		std::string text = ReplaceAll(iter->second.text, "\n", "\\n");
		section->Set(iter->first, text);
	}

	cat->ClearMissed();
}
