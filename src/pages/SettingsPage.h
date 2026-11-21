#pragma once

#include "app/Page.h"
#include "app/Widgets.h"

class AppButton;
class BrowseField;
class LineEdit;
class SegmentedControl;
class StyledLabel;
class ToggleSwitch;

/*
 * 设置: appearance (theme / backdrop), injection defaults, log source and
 * advanced actions.
 */
class SettingsPage : public Page
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

signals:
    void restartAsAdminRequested();
    void openSettingsDirRequested();
    void resetRequested();

private:
    QWidget* sectionCard(const QString& title);
    QWidget* row(const QString& caption, const QString& hint, QWidget* control,
                 bool separator = true);

    SegmentedControl* m_theme = nullptr;
    SegmentedControl* m_backdrop = nullptr;
    StyledLabel* m_backdropHint = nullptr;
    BrowseField* m_dll = nullptr;
    LineEdit* m_keyword = nullptr;
    ToggleSwitch* m_autoSwitch = nullptr;
    ToggleSwitch* m_toastOnDone = nullptr;
    BrowseField* m_logOverride = nullptr;
    AppButton* m_reset = nullptr;
    bool m_resetArmed = false;
};
