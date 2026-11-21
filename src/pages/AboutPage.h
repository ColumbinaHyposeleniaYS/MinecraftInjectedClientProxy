#pragma once

#include "app/Page.h"
#include "app/Widgets.h"

class StyledLabel;

/*
 * 关于: product identity, runtime requirements, quick start summary and
 * build information.
 */
class AboutPage : public Page
{
    Q_OBJECT

public:
    explicit AboutPage(QWidget* parent = nullptr);
};
