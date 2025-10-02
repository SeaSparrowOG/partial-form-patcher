#pragma once

namespace Settings
{
	namespace INI
	{
		bool Read();

		class Holder :
			public REX::Singleton<Holder>
		{
		public:
			bool StoreSettings();
			void DumpSettings();

			template <typename T>
			std::optional<T> GetStoredSetting(const std::string& a_settingName) {
				if constexpr (std::is_same_v<T, float>) {
					auto it = floatSettings.find(a_settingName);
					if (it != floatSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, std::string>) {
					auto it = stringSettings.find(a_settingName);
					if (it != stringSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, long>) {
					auto it = longSettings.find(a_settingName);
					if (it != longSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, bool>) {
					auto it = boolSettings.find(a_settingName);
					if (it != boolSettings.end()) return it->second;
				}
				else {
					static_assert(always_false<T>, "Called GetStoredSetting with unsupported type.");
				}
				return std::nullopt;
			}

		private:
			std::map<std::string, long>        longSettings;
			std::map<std::string, bool>        boolSettings;
			std::map<std::string, float>       floatSettings;
			std::map<std::string, std::string> stringSettings;

			bool OverrideSettings();
		};

		inline static constexpr const char* FAKE_SETTING = "Fake|bSetting";

		inline static constexpr const std::uint8_t EXPECTED_COUNT = 1;

		inline static constexpr const std::array<const char*, EXPECTED_COUNT> EXPECTED_SETTINGS = {
			FAKE_SETTING
		};

		template <typename T>
		std::optional<T> GetSetting(const std::string& a_settingName) {
			auto* holder = Holder::GetSingleton();
			return holder ? holder->GetStoredSetting<T>(a_settingName) : std::nullopt;
		}
	}
}