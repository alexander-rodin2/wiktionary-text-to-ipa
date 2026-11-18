/*
 wiktionary-text-to-ipa.cpp is a program that fetches pronunciations from Wiktionary
 Copyright (C) 2026  Alexander Rodin

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

QByteArray fetchUrl(const QUrl &url);
void extractIpa(const QString &wikiText, QString &receivedPronunciation, QString &receivedPronunciationSound, QString &generalAmerican, QString &generalAmericanSound);
void fetchSoundData(const QString &filename, QString &url, QString &license);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("word", "The word the IPA transcription is to be looked up");
    QCommandLineOption withSoundsOption("with-sounds", "Add links to the sounds.");
    parser.addOption(withSoundsOption);
    parser.addHelpOption();
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp();
    bool withSounds = parser.isSet(withSoundsOption);

    QString word = args[0];
    QUrl url = "https://en.wiktionary.org/w/rest.php/v1/page/" + word;
    QByteArray response = fetchUrl(url);
    QJsonDocument responseDocument = QJsonDocument::fromJson(response);
    QJsonObject responseObject = responseDocument.object();
    QString source = responseObject.value("source").toString();

    QString receivedPronunciation;
    QString receivedPronunciationSound;
    QString generalAmerican;
    QString generalAmericanSound;
    extractIpa(source, receivedPronunciation, receivedPronunciationSound, generalAmerican, generalAmericanSound);

    QJsonObject outputObject {
        { "word", word },
        { "receivedPronunciation", receivedPronunciation },
        { "generalAmerican", generalAmerican },
    };
    if (withSounds)
    {
        QString receivedPronunciationSoundUrl;
        QString receivedPronunciationSoundLicense;
        fetchSoundData(receivedPronunciationSound, receivedPronunciationSoundUrl, receivedPronunciationSoundLicense);
        outputObject.insert("receivedPronunciationSoundUrl", receivedPronunciationSoundUrl);
        outputObject.insert("receivedPronunciationSoundLicense", receivedPronunciationSoundLicense);

        QString generalAmericanSoundUrl;
        QString generalAmericanSoundLicense;
        fetchSoundData(generalAmericanSound, generalAmericanSoundUrl, generalAmericanSoundLicense);
        outputObject.insert("generalAmericanSoundUrl", generalAmericanSoundUrl);
        outputObject.insert("generalAmericanSoundLicense", generalAmericanSoundLicense);
    }
    QJsonDocument outputDocument(outputObject);

    QFile outputFile;
    (void)outputFile.open(stdout, QIODeviceBase::WriteOnly);
    outputFile.write(outputDocument.toJson(QJsonDocument::Compact));
    outputFile.write("\n");
}

QByteArray fetchUrl(const QUrl &url)
{
    QEventLoop eventLoop;
    QNetworkAccessManager manager;
    QByteArray response;
    QObject::connect(&manager, &QNetworkAccessManager::finished, [&](QNetworkReply *reply) {
        response = reply->readAll();
        reply->deleteLater();
        eventLoop.exit();
    });
    QNetworkRequest request(url);
    manager.get(request);
    eventLoop.exec();
    return response;
}

struct IpaTemplate
{
    QString language;
    QString ipa;
    QSet<QString> accents;
};

void parseIpaTemplate(QString templateString, IpaTemplate &ipaTemplate)
{
    if (! templateString.startsWith("{{"))
        return;
    if (! templateString.endsWith("}}"))
        return;
    templateString.slice(2, templateString.length() - 2);
    templateString.slice(0, templateString.length() - 2);
    QStringList tokens = templateString.split("|");
    if (tokens.size() < 3)
        return;
    if (tokens[0] != "IPA")
        return;
    ipaTemplate.language = tokens[1];
    ipaTemplate.ipa = tokens[2];
    if (ipaTemplate.ipa.startsWith("/") || ipaTemplate.ipa.startsWith("["))
        ipaTemplate.ipa.slice(1, ipaTemplate.ipa.length() - 1);
    if (ipaTemplate.ipa.endsWith("/") || ipaTemplate.ipa.endsWith("]"))
        ipaTemplate.ipa.slice(0, ipaTemplate.ipa.length() - 1);
    QString &lastToken = tokens[tokens.size() - 1];
    if (lastToken.startsWith("a=") || lastToken.startsWith("aa="))
    {
        if (lastToken.startsWith("a="))
            lastToken.slice(2, lastToken.length() - 2);
        if (lastToken.startsWith("aa="))
            lastToken.slice(3, lastToken.length() - 3);
        QStringList accents = lastToken.split(",");
        for (const QString &accent: accents)
            ipaTemplate.accents.insert(accent);
    }
}

struct EnPrTemplate
{
    QSet<QString> accents;
};

void parseEnPrTemplate(QString templateString, EnPrTemplate &enPrTemplate)
{
    if (! templateString.startsWith("{{"))
        return;
    if (! templateString.endsWith("}}"))
        return;
    templateString.slice(2, templateString.length() - 2);
    templateString.slice(0, templateString.length() - 2);
    QStringList tokens = templateString.split("|");
    if (tokens.size() < 2)
        return;
    if (tokens[0] != "enPR")
        return;
    QString &lastToken = tokens[tokens.size() - 1];
    if (lastToken.startsWith("a=") || lastToken.startsWith("aa="))
    {
        if (lastToken.startsWith("a="))
            lastToken.slice(2, lastToken.length() - 2);
        if (lastToken.startsWith("aa="))
            lastToken.slice(3, lastToken.length() - 3);
        QStringList accents = lastToken.split(",");
        for (const QString &accent: accents)
            enPrTemplate.accents.insert(accent);
    }
}

struct AudioTemplate
{
    QString language;
    QString filename;
    QSet<QString> accents;
};

void parseAudioTemplate(QString templateString, AudioTemplate &audioTemplate)
{
    if (! templateString.startsWith("{{"))
        return;
    if (! templateString.endsWith("}}"))
        return;
    templateString.slice(2, templateString.length() - 2);
    templateString.slice(0, templateString.length() - 2);
    QStringList tokens = templateString.split("|");
    if (tokens.size() < 3)
        return;
    if (tokens[0] != "audio")
        return;
    audioTemplate.language = tokens[1];
    audioTemplate.filename = tokens[2];
    QString &lastToken = tokens[tokens.size() - 1];
    if (lastToken.startsWith("a=") || lastToken.startsWith("aa="))
    {
        if (lastToken.startsWith("a="))
            lastToken.slice(2, lastToken.length() - 2);
        if (lastToken.startsWith("aa="))
            lastToken.slice(3, lastToken.length() - 3);
        QStringList accents = lastToken.split(",");
        for (const QString &accent: accents)
            audioTemplate.accents.insert(accent);
    }
}

void extractIpa(const QString &wikiText, QString &receivedPronunciation, QString &receivedPronunciationSound, QString &generalAmerican, QString &generalAmericanSound)
{
    QStringList lines = wikiText.split("\n");

    QString currentLanguage;
    QMap<QString, QStringList> linesByLanguage;
    for (const QString &line: lines)
    {
        if (line.startsWith("==") && line.length() >= 3 && line[2].isLetter())
        {
            currentLanguage = line;
            continue;
        }
        linesByLanguage[currentLanguage] << line;
    }

    if (! linesByLanguage.contains("==English=="))
        return;

    QString currentSubsection;
    QMap<QString, QStringList> linesBySubsection;
    for (const QString &line: linesByLanguage["==English=="])
    {
        QRegularExpression regex("^=+([A-Za-z ]*)=+$");
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch())
        {
            currentSubsection = match.captured(1);
            continue;
        }
        linesBySubsection[currentSubsection] << line;
    }

    if (! linesBySubsection.contains("Pronunciation"))
        return;

    auto getIpaForPronunciation = [](const QStringList &lines, const QStringList &pronunciations, QString &ipa, QString &sound) {
        for (const QString &line: lines)
        {
            QRegularExpression audioRegex("^.*(\\{\\{audio\\|[^\\}]*\\}\\}).*$");
            QRegularExpressionMatch audioMatch = audioRegex.match(line);
            if (audioMatch.hasMatch())
            {
                AudioTemplate audioTemplate;
                parseAudioTemplate(audioMatch.captured(1), audioTemplate);
                for (const QString &pronunciation: pronunciations)
                {
                    if (audioTemplate.accents.contains(pronunciation))
                        sound = audioTemplate.filename;
                }
            }
        }

        QStringList pronunciationSections;
        QMap<QString, QStringList> linesByPronunciationSections;
        for (const QString &line: lines)
        {
            QRegularExpression regex("^.*\{\{a\\|en\\|([A-Za-z \\|]+)\\}\\}.*$");
            QRegularExpressionMatch match = regex.match(line);
            if (match.hasMatch())
            {
                pronunciationSections = match.captured(1).split("|");
                continue;
            }
            for (const QString &pronunciationSection: pronunciationSections)
                linesByPronunciationSections[pronunciationSection] << line;
        }
        for (const QString &pronunciation: pronunciations)
        {
            if (linesByPronunciationSections.contains(pronunciation))
            {
                for (const QString &line: linesByPronunciationSections[pronunciation])
                {
                    QRegularExpression ipaRegex("^.*(\\{\\{IPA\\|[^\\}]*\\}\\}).*$");
                    QRegularExpressionMatch ipaMatch = ipaRegex.match(line);
                    if (ipaMatch.hasMatch())
                    {
                        IpaTemplate ipaTemplate;
                        parseIpaTemplate(ipaMatch.captured(1), ipaTemplate);
                        if (ipaTemplate.accents.isEmpty() || ipaTemplate.accents.contains(pronunciation) || ipaTemplate.accents.contains("strong form"))
                        {
                            ipa = ipaTemplate.ipa;
                            return;
                        }
                    }
                }
            }
        }

        for (const QString &pronunciation: pronunciations)
        {
            for (const QString &line: lines)
            {
                QRegularExpression ipaRegex("^.*(\\{\\{IPA\\|[^\\}]*\\}\\}).*$");
                QRegularExpression enPrRegex("^.*(\\{\\{enPR\\|[^\\}]*\\}\\}).*$");

                QRegularExpressionMatch ipaMatch = ipaRegex.match(line);
                QRegularExpressionMatch enPrMatch = enPrRegex.match(line);

                IpaTemplate ipaTemplate;
                EnPrTemplate enPrTemplate;

                if (ipaMatch.hasMatch())
                    parseIpaTemplate(ipaMatch.captured(1), ipaTemplate);
                if (enPrMatch.hasMatch())
                    parseEnPrTemplate(enPrMatch.captured(1), enPrTemplate);

                if (ipaTemplate.accents.contains(pronunciation) ||
                    (ipaTemplate.accents.isEmpty() && enPrTemplate.accents.contains(pronunciation)))
                {
                    ipa = ipaTemplate.ipa;
                    return;
                }
            }
        }

        for (const QString &line: lines)
        {
            QRegularExpression ipaRegex("^.*(\\{\\{IPA\\|[^\\}]*\\}\\}).*$");
            QRegularExpressionMatch ipaMatch = ipaRegex.match(line);
            if (ipaMatch.hasMatch())
            {
                IpaTemplate ipaTemplate;
                parseIpaTemplate(ipaMatch.captured(1), ipaTemplate);
                ipa = ipaTemplate.ipa;
                return;
            }
        }
    };

    getIpaForPronunciation(linesBySubsection["Pronunciation"], { "RP", "UK" }, receivedPronunciation, receivedPronunciationSound);
    getIpaForPronunciation(linesBySubsection["Pronunciation"], { "GA", "GenAm", "US" }, generalAmerican, generalAmericanSound);
}

void fetchSoundData(const QString &filename, QString &soundUrl, QString &soundLicense)
{
    if (filename.isEmpty())
        return;

    QByteArray response = fetchUrl(QUrl("https://en.wiktionary.org/wiki/File:" + filename));
    QString html = QString::fromUtf8(response);
    QRegularExpression divRegex("<div class=\"fullMedia\">(.*)</div>");
    QRegularExpressionMatch divMatch = divRegex.match(html);
    if (divMatch.hasMatch())
    {
        QString divContent = divMatch.captured(1);

        QRegularExpression hrefRegex("^.*<a href=(\".*\")>.*$");
        QRegularExpressionMatch hrefMatch = hrefRegex.match(divContent);
        if (hrefMatch.hasMatch())
        {
            QString urlString = hrefMatch.captured(1).split(" ")[0];
            urlString.slice(1, urlString.length() - 2);
            QUrl url(urlString);
            url.setScheme("https");
            soundUrl = url.toString(QUrl::RemoveQuery);
        }
    }
    soundLicense = "https://commons.wikimedia.org/wiki/File:" + filename;
}
