#pragma once

#include <KCModule>

class QLineEdit;
class QSpinBox;

class LlmRunnerKcm : public KCModule
{
    Q_OBJECT

public:
    LlmRunnerKcm(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;
    void defaults() override;

private:
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_apiUrl = nullptr;
    QLineEdit *m_model = nullptr;
    QSpinBox *m_debounceMs = nullptr;
    QLineEdit *m_trigger = nullptr;
};
