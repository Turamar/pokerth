#pragma once

#include <QObject>
#include <QString>
#include <QQmlApplicationEngine>
#include <QTranslator>

class LanguageManager : public QObject
{
	Q_OBJECT
public:
	// startupTranslator is the translator the client installed before the QML
	// engine ran (the configured language, see qmlTranslationFile() in
	// pokerth.cpp). It is removed on the first switchLanguage() so that only one
	// language is ever in effect.
	explicit LanguageManager(QQmlApplicationEngine *engine,
							 QTranslator *startupTranslator = nullptr,
							 QObject *parent = nullptr);

	// Invokable from QML to switch the app language at runtime
	Q_INVOKABLE void switchLanguage(const QString &langCode);

private:
	QQmlApplicationEngine *m_engine;
	QTranslator m_translator;
	QTranslator *m_startupTranslator = nullptr;
};
