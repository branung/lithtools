#include "fontview.h"

#include <QFontDatabase>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

FontView::FontView(QWidget* parent) : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    title_ = new QLabel(this);
    title_->setWordWrap(true);
    title_->setContentsMargins(6, 4, 6, 4);
    title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sample_ = new QTextEdit(this);
    sample_->setReadOnly(true);
    sample_->setToolTip(tr("Type to try the font"));
    l->addWidget(title_);
    l->addWidget(sample_, 1);
}

FontView::~FontView() { clear(); }

void FontView::clear() {
    if (id_ >= 0) QFontDatabase::removeApplicationFont(id_);
    id_ = -1;
    title_->clear();
    sample_->clear();
}

bool FontView::loadFont(const QByteArray& data, const QString& name, QString* error) {
    clear();
    id_ = QFontDatabase::addApplicationFontFromData(data);
    QStringList families = id_ >= 0 ? QFontDatabase::applicationFontFamilies(id_) : QStringList();
    if (families.isEmpty()) {
        if (id_ >= 0) QFontDatabase::removeApplicationFont(id_);
        id_ = -1;
        if (error) *error = tr("Can't read %1: Qt doesn't take it as a font").arg(name);
        return false;
    }
    QString family = families.first();
    QStringList styles = QFontDatabase::styles(family);
    title_->setText(tr("<b>%1</b> &nbsp;font: %2").arg(name.toHtmlEscaped(), families.join(QStringLiteral(", ")).toHtmlEscaped()) +
                    (styles.isEmpty() ? QString() : tr("<br>Styles: %1").arg(styles.join(QStringLiteral(", ")).toHtmlEscaped())));
    QString ascii;
    for (int c = 32; c < 127; ++c) {
        ascii += QChar(c);
        if (c % 32 == 31) ascii += QLatin1Char('\n');
    }
    QString h;
    auto line = [&](int pt, const QString& text) {
        h += QStringLiteral("<p style=\"font-family:'%1'; font-size:%2pt; margin:2px\">%3</p>")
                 .arg(family.toHtmlEscaped()).arg(pt).arg(text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    };
    line(14, ascii);
    for (int pt : {12, 18, 24, 36, 48}) line(pt, tr("%1 pt: The quick brown fox jumps over the lazy dog").arg(pt));
    sample_->setHtml(h);
    sample_->setReadOnly(false);   // try it out; nothing is saved
    return true;
}
