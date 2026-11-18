#include "retranslate.h"
#include <QCoreApplication>

LanguageManager::LanguageManager(QQmlApplicationEngine *engine,
								 QTranslator *startupTranslator,
								 QObject *parent)
	: QObject(parent)
	, m_engine(engine)
	, m_startupTranslator(startupTranslator)
{
}

void LanguageManager::switchLanguage(const QString &langCode)
{
	// From here on the QML side owns the language: drop the translator the
	// client installed before the engine ran (the configured language). Stacked
	// translators are why an English UI used to show Russian - the system-locale
	// one stayed installed underneath and answered every string the English .qm
	// did not have. Without it a missing string falls back to the source.
	if (m_startupTranslator) {
		QCoreApplication::removeTranslator(m_startupTranslator);
		delete m_startupTranslator;
		m_startupTranslator = nullptr;
	}

	// Remove the currently installed translator
	QCoreApplication::removeTranslator(&m_translator);

	// Build the QM file path (ensure it matches your RESOURCE_PREFIX)
	const QString qmFile = QStringLiteral(":/i18n/pokerth_%1.qm").arg(langCode);
	if (m_translator.load(qmFile)) {
		QCoreApplication::installTranslator(&m_translator);
		qDebug() << "Installed new Translator with code " << langCode;
	} else {
		qWarning() << "Failed to load translation file:" << qmFile;
	}

	// Retranslate all QML bindings
	m_engine->retranslate();
}
