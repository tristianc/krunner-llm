#pragma once

#include <KRunner/AbstractRunner>
#include <QString>

class LlmRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    LlmRunner(QObject *parent, const KPluginMetaData &data);

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
    void reloadConfiguration() override;

protected:
    void init() override;

private:
    QString askLlm(const QString &prompt, const KRunner::RunnerContext &context) const;

    QString m_triggerWord;
    QString m_model;
    QString m_apiKey;
    QString m_apiUrl;
    int m_debounceMs = 600;
};
