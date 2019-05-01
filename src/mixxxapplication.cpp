#include <QtDebug>
#include <QTouchEvent>
#include "mixxxapplication.h"

#include "library/crate/crateid.h"
#include "control/controlproxy.h"
#include "mixxx.h"

// When linking Qt statically on Windows we have to Q_IMPORT_PLUGIN all the
// plugins we link in build/depends.py.
#ifdef QT_NODLL
#include <QtPlugin>
#if QT_VERSION >= 0x050000
// sqldrivers plugins
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
// platform plugins
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
// style plugins
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
// imageformats plugins
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
Q_IMPORT_PLUGIN(QTgaPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QGifPlugin)
// accessible plugins
// TODO(rryan): This is supposed to exist but does not in our builds.
//Q_IMPORT_PLUGIN(AccessibleFactory)
#else 
// iconengines plugins
Q_IMPORT_PLUGIN(qsvgicon)
// imageformats plugins
Q_IMPORT_PLUGIN(qsvg)
Q_IMPORT_PLUGIN(qico)
Q_IMPORT_PLUGIN(qtga)
// accessible plugins
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif
#endif


MixxxApplication::MixxxApplication(int& argc, char** argv)
        : QApplication(argc, argv),
          m_fakeMouseSourcePointId(0),
          m_fakeMouseWidget(NULL),
          m_activeTouchButton(Qt::NoButton),
          m_pTouchShift(NULL) {
    registerMetaTypes();
}

MixxxApplication::~MixxxApplication() {
}

void MixxxApplication::registerMetaTypes() {
    // Register custom data types for signal processing
    qRegisterMetaType<TrackId>("TrackId");
    qRegisterMetaType<QList<TrackId>>("QList<TrackId>");
    qRegisterMetaType<QSet<TrackId>>("QSet<TrackId>");
    qRegisterMetaType<CrateId>("CrateId");
    qRegisterMetaType<QList<CrateId>>("QList<CrateId>");
    qRegisterMetaType<QSet<CrateId>>("QSet<CrateId>");
    qRegisterMetaType<TrackPointer>("TrackPointer");
    qRegisterMetaType<mixxx::ReplayGain>("mixxx::ReplayGain");
    qRegisterMetaType<mixxx::Bpm>("mixxx::Bpm");
    qRegisterMetaType<mixxx::Duration>("mixxx::Duration");
}

bool MixxxApplication::touchIsRightButton() {
    if (!m_pTouchShift) {
        m_pTouchShift = new ControlProxy(
                "[Controls]", "touch_shift", this);
    }
    return (m_pTouchShift->get() != 0.0);
}
