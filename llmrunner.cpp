#include "llmrunner.h"
#include "defaults.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <KRunner/RunnerSyntax>

#include <QClipboard>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextList>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace
{
constexpr int kPollMs = 25;
constexpr int kHttpTimeoutMs = 30000;
const QString kSystemPrompt = QStringLiteral(
    "You are a concise assistant embedded in a desktop launcher. "
    "Answer in 1-3 short paragraphs unless the user explicitly asks for more.");

QString buildEndpoint(QString base)
{
    if (base.isEmpty()) {
        base = KrunnerLlm::kDefaultApiUrl;
    }
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (!base.endsWith(QLatin1String("/chat/completions"))) {
        base += QLatin1String("/chat/completions");
    }
    return base;
}

// Multi-line matches render as Qt QML Text.StyledText, which ignores
// inline CSS — so we cannot ship QTextDocument::toHtml() verbatim.
// Instead we walk the QTextDocument from setMarkdown() and emit the
// documented StyledText subset: <b> <i> <code> <pre> <a> <s> <u> <br>.
QString markdownToStyled(const QString &md)
{
    QTextDocument doc;
    doc.setMarkdown(md);

    QString out;
    bool firstBlock = true;

    for (QTextBlock block = doc.begin(); block != doc.end(); block = block.next()) {
        if (!firstBlock) {
            out += QStringLiteral("<br>");
        }
        firstBlock = false;

        const QTextBlockFormat bf = block.blockFormat();

        // Fenced / indented code block → render as <pre>, verbatim.
        if (bf.hasProperty(QTextFormat::BlockCodeFence)
            || bf.hasProperty(QTextFormat::BlockCodeLanguage)
            || bf.nonBreakableLines()) {
            out += QStringLiteral("<pre>") + block.text().toHtmlEscaped() + QStringLiteral("</pre>");
            continue;
        }

        // List item marker.
        if (QTextList *list = block.textList()) {
            if (list->format().style() == QTextListFormat::ListDecimal) {
                out += QString::number(list->itemNumber(block) + 1) + QStringLiteral(". ");
            } else {
                out += QStringLiteral("• ");
            }
        }

        // Walk fragments, applying per-char styling.
        QString blockHtml;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) {
                continue;
            }
            QString t = frag.text().toHtmlEscaped();
            const QTextCharFormat fmt = frag.charFormat();

            if (fmt.fontFixedPitch()) {
                t = QStringLiteral("<code>") + t + QStringLiteral("</code>");
            }
            if (fmt.fontItalic()) {
                t = QStringLiteral("<i>") + t + QStringLiteral("</i>");
            }
            if (fmt.fontWeight() >= QFont::Bold) {
                t = QStringLiteral("<b>") + t + QStringLiteral("</b>");
            }
            if (fmt.fontStrikeOut()) {
                t = QStringLiteral("<s>") + t + QStringLiteral("</s>");
            }
            if (fmt.fontUnderline()) {
                t = QStringLiteral("<u>") + t + QStringLiteral("</u>");
            }
            if (fmt.isAnchor() && !fmt.anchorHref().isEmpty()) {
                t = QStringLiteral("<a href=\"") + fmt.anchorHref().toHtmlEscaped()
                    + QStringLiteral("\">") + t + QStringLiteral("</a>");
            }
            blockHtml += t;
        }

        // Headings: just embolden; <h1>-<h6> would scale the row text huge.
        if (bf.headingLevel() > 0) {
            blockHtml = QStringLiteral("<b>") + blockHtml + QStringLiteral("</b>");
        }

        out += blockHtml;
    }

    return out;
}
}

LlmRunner::LlmRunner(QObject *parent, const KPluginMetaData &data)
    : KRunner::AbstractRunner(parent, data)
{
}

void LlmRunner::init()
{
    reloadConfiguration();
}

void LlmRunner::reloadConfiguration()
{
    KConfigGroup c = config();
    m_triggerWord = c.readEntry("trigger", KrunnerLlm::kDefaultTrigger);
    m_model = c.readEntry("model", KrunnerLlm::kDefaultModel);
    m_apiKey = c.readEntry("apiKey", QString());
    m_apiUrl = c.readEntry("apiUrl", KrunnerLlm::kDefaultApiUrl);
    m_debounceMs = c.readEntry("debounceMs", KrunnerLlm::kDefaultDebounceMs);

    setMinLetterCount(m_triggerWord.length() + 2);

    QList<KRunner::RunnerSyntax> syntaxes;
    syntaxes << KRunner::RunnerSyntax(QStringLiteral("%1 :q:").arg(m_triggerWord),
                                      i18n("Ask an LLM for an answer to :q:"));
    setSyntaxes(syntaxes);
}

QString LlmRunner::askLlm(const QString &prompt, const KRunner::RunnerContext &context) const
{
    QNetworkAccessManager nam;

    QNetworkRequest req{QUrl(buildEndpoint(m_apiUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), kSystemPrompt}});
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), prompt}});

    QJsonObject body{
        {QStringLiteral("model"), m_model},
        {QStringLiteral("messages"), messages},
    };

    QNetworkReply *reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer abortPoll;
    abortPoll.setInterval(kPollMs);
    QObject::connect(&abortPoll, &QTimer::timeout, &loop, [&] {
        if (!context.isValid()) {
            reply->abort();
        }
    });
    abortPoll.start();

    QTimer overallTimeout;
    overallTimeout.setSingleShot(true);
    QObject::connect(&overallTimeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    overallTimeout.start(kHttpTimeoutMs);

    loop.exec();

    const auto netErr = reply->error();
    const QString errStr = reply->errorString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool wasAborted = (netErr == QNetworkReply::OperationCanceledError);
    const QByteArray raw = wasAborted ? QByteArray{} : reply->readAll();
    reply->deleteLater();

    if (!context.isValid()) {
        return {};
    }

    const auto rootObj = QJsonDocument::fromJson(raw).object();
    const QString apiErrorMsg = rootObj.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();

    if (netErr != QNetworkReply::NoError) {
        switch (httpStatus) {
        case 401:
            return i18n("Invalid API key. Open the LLM settings to update it.");
        case 402:
            return apiErrorMsg.isEmpty() ? i18n("Payment required (402). Top up your account or switch provider.") : apiErrorMsg;
        case 403:
            return apiErrorMsg.isEmpty() ? i18n("Request refused (403).") : apiErrorMsg;
        case 404:
            return i18n("Model \"%1\" not found at the configured API URL.", m_model);
        case 408:
            return i18n("API request timed out.");
        case 429:
            return apiErrorMsg.isEmpty() ? i18n("Rate limited. Try again shortly.") : apiErrorMsg;
        case 0:
            return i18n("Network error: %1", errStr);
        default:
            if (httpStatus >= 500 && httpStatus < 600) {
                return apiErrorMsg.isEmpty() ? i18n("Server error (%1).", httpStatus) : apiErrorMsg;
            }
            if (!apiErrorMsg.isEmpty()) {
                return apiErrorMsg;
            }
            return httpStatus > 0 ? i18n("Error (HTTP %1): %2", httpStatus, errStr) : i18n("Error: %1", errStr);
        }
    }

    const auto choices = rootObj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return apiErrorMsg.isEmpty() ? i18n("No response from the API.") : apiErrorMsg;
    }
    const QString content = choices.first()
                                .toObject()
                                .value(QStringLiteral("message"))
                                .toObject()
                                .value(QStringLiteral("content"))
                                .toString()
                                .trimmed();
    return content.isEmpty() ? i18n("Empty response from %1.", m_model) : content;
}

void LlmRunner::match(KRunner::RunnerContext &context)
{
    QString query = context.query();
    const QString prefix = m_triggerWord + QLatin1Char(' ');
    if (!query.startsWith(prefix, Qt::CaseInsensitive)) {
        return;
    }
    const QString prompt = query.mid(prefix.length()).trimmed();
    if (prompt.isEmpty()) {
        return;
    }

    QDeadlineTimer debounce(m_debounceMs);
    while (!debounce.hasExpired()) {
        if (!context.isValid()) {
            return;
        }
        QThread::msleep(qMin<int>(kPollMs, debounce.remainingTime() + 1));
    }
    if (!context.isValid()) {
        return;
    }

    if (m_apiKey.isEmpty()) {
        KRunner::QueryMatch m(this);
        m.setText(i18n("No API key set"));
        m.setSubtext(i18n("Open System Settings → Search → LLM and paste your key"));
        m.setIconName(QStringLiteral("dialog-warning"));
        m.setRelevance(1.0);
        context.addMatch(m);
        return;
    }

    const QString answer = askLlm(prompt, context);
    if (!context.isValid() || answer.isEmpty()) {
        return;
    }

    KRunner::QueryMatch m(this);
    m.setText(markdownToStyled(answer));
    m.setSubtext(i18n("via %1 — press Enter to copy", m_model));
    m.setData(answer);
    m.setIconName(QStringLiteral("tools-wizard"));
    m.setMultiLine(true);
    m.setRelevance(1.0);
    m.setCategoryRelevance(KRunner::QueryMatch::CategoryRelevance::Highest);
    context.addMatch(m);
}

void LlmRunner::run(const KRunner::RunnerContext & /*context*/, const KRunner::QueryMatch &match)
{
    if (auto *cb = QGuiApplication::clipboard()) {
        cb->setText(match.data().toString());
    }
}

K_PLUGIN_CLASS_WITH_JSON(LlmRunner, "llmrunner.json")

#include "llmrunner.moc"
