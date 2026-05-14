#pragma once

#include <QLatin1String>
#include <QString>

namespace KrunnerLlm
{
inline constexpr auto kConfigGroupRunners = QLatin1String("Runners");
inline constexpr auto kConfigGroupPlugin = QLatin1String("krunner_llm");

inline const QString kDefaultApiUrl = QStringLiteral("https://openrouter.ai/api/v1");
inline const QString kDefaultModel = QStringLiteral("deepseek/deepseek-v4-flash");
inline const QString kDefaultTrigger = QStringLiteral("ai");
inline constexpr int kDefaultDebounceMs = 600;
}
