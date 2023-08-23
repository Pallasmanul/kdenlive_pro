/*
    SPDX-FileCopyrightText: 2013 Meltytech LLC
    SPDX-FileCopyrightText: 2013 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "utils/qcolorutils.h"

#include <QPainter>
#include <QResizeEvent>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;

class WheelContainer : public QWidget
{
    Q_OBJECT
public:
    explicit WheelContainer(QString id, QString name, NegQColor color, int unitSize, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    NegQColor color() const;
    void setColor(const QList <double> &values);
    void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);
    const QList <double> getNiceParamValues() const;
    void setRedColor(double value);
    void setGreenColor(double value);
    void setBlueColor(double value);

public Q_SLOTS:
    void changeColor(const NegQColor &color);

Q_SIGNALS:
    void colorChange(const NegQColor &color);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_id;
    QSize m_initialSize;
    QImage m_image;
    bool m_isMouseDown;
    QPointF m_lastPoint;
    int m_margin;
    int m_sliderWidth;
    int m_sliderBorder;
    QRegion m_wheelRegion;
    QRegion m_sliderRegion;
    NegQColor m_color;
    int m_unitSize;
    QString m_name;
    bool m_wheelClick;
    bool m_sliderClick;
    bool m_sliderFocus;

    qreal m_sizeFactor = 1;
    qreal m_defaultValue = 1;
    qreal m_zeroShift = 0;

    int wheelSize() const;
    int sliderHeight() const;
    NegQColor colorForPoint(const QPointF &point);
    QPointF pointForColor();
    double yForColor();
    void drawWheel();
    void drawWheelDot(QPainter &painter);
    void drawSliderBar(QPainter &painter);
    void drawSlider();
    const QString getParamValues() const;
};

class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(const QString &id, const QString &name, const NegQColor &color, QWidget *parent = nullptr);
    NegQColor color() const;
    void setColor(const QList <double> &values);
    void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);

private:
    WheelContainer *m_container;
    QLabel *m_wheelName;
    QDoubleSpinBox *m_redEdit;
    QDoubleSpinBox *m_greenEdit;
    QDoubleSpinBox *m_blueEdit;

Q_SIGNALS:
    void colorChange(const NegQColor &color);
};
