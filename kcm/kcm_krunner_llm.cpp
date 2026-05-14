#include "kcm_krunner_llm.h"
#include "../defaults.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

LlmRunnerKcm::LlmRunnerKcm(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto *form = new QFormLayout(widget());

    m_apiKey = new QLineEdit(widget());
    m_apiKey->setEchoMode(QLineEdit::Password);
    form->addRow(i18n("API key:"), m_apiKey);

    m_apiUrl = new QLineEdit(widget());
    m_apiUrl->setPlaceholderText(KrunnerLlm::kDefaultApiUrl);
    form->addRow(i18n("API URL:"), m_apiUrl);

    m_model = new QLineEdit(widget());
    m_model->setPlaceholderText(KrunnerLlm::kDefaultModel);
    form->addRow(i18n("Model:"), m_model);

    m_debounceMs = new QSpinBox(widget());
    m_debounceMs->setRange(0, 5000);
    m_debounceMs->setSingleStep(50);
    m_debounceMs->setSuffix(i18n(" ms"));
    form->addRow(i18n("Debounce:"), m_debounceMs);

    m_trigger = new QLineEdit(widget());
    m_trigger->setPlaceholderText(KrunnerLlm::kDefaultTrigger);
    form->addRow(i18n("Trigger word:"), m_trigger);

    auto *hint = new QLabel(
        i18n("Type \"<trigger> <question>\" in KRunner to query the LLM. "
             "The answer is shown as a match; press Enter to copy it to the clipboard. "
             "API URL must be an OpenAI-compatible chat-completions endpoint base "
             "(OpenRouter, OpenAI, Groq, Ollama, etc)."),
        widget());
    hint->setWordWrap(true);
    hint->setEnabled(false);
    form->addRow(hint);

    auto markDirty = [this] { setNeedsSave(true); };
    connect(m_apiKey, &QLineEdit::textChanged, this, markDirty);
    connect(m_apiUrl, &QLineEdit::textChanged, this, markDirty);
    connect(m_model, &QLineEdit::textChanged, this, markDirty);
    connect(m_debounceMs, qOverload<int>(&QSpinBox::valueChanged), this, markDirty);
    connect(m_trigger, &QLineEdit::textChanged, this, markDirty);
}

void LlmRunnerKcm::load()
{
    auto cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup g = cfg->group(KrunnerLlm::kConfigGroupRunners).group(KrunnerLlm::kConfigGroupPlugin);
    m_apiKey->setText(g.readEntry("apiKey", QString()));
    m_apiUrl->setText(g.readEntry("apiUrl", KrunnerLlm::kDefaultApiUrl));
    m_model->setText(g.readEntry("model", KrunnerLlm::kDefaultModel));
    m_debounceMs->setValue(g.readEntry("debounceMs", KrunnerLlm::kDefaultDebounceMs));
    m_trigger->setText(g.readEntry("trigger", KrunnerLlm::kDefaultTrigger));
    setNeedsSave(false);
}

void LlmRunnerKcm::save()
{
    auto cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup g = cfg->group(KrunnerLlm::kConfigGroupRunners).group(KrunnerLlm::kConfigGroupPlugin);
    g.writeEntry("apiKey", m_apiKey->text());
    g.writeEntry("apiUrl", m_apiUrl->text().isEmpty() ? KrunnerLlm::kDefaultApiUrl : m_apiUrl->text());
    g.writeEntry("model", m_model->text().isEmpty() ? KrunnerLlm::kDefaultModel : m_model->text());
    g.writeEntry("debounceMs", m_debounceMs->value());
    g.writeEntry("trigger", m_trigger->text().isEmpty() ? KrunnerLlm::kDefaultTrigger : m_trigger->text());
    g.sync();
    setNeedsSave(false);
}

void LlmRunnerKcm::defaults()
{
    m_apiKey->clear();
    m_apiUrl->setText(KrunnerLlm::kDefaultApiUrl);
    m_model->setText(KrunnerLlm::kDefaultModel);
    m_debounceMs->setValue(KrunnerLlm::kDefaultDebounceMs);
    m_trigger->setText(KrunnerLlm::kDefaultTrigger);
}

K_PLUGIN_CLASS(LlmRunnerKcm)

#include "kcm_krunner_llm.moc"
