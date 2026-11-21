#pragma once

#include "app/Page.h"
#include "app/Widgets.h"
#include "core/InjectionSequence.h"

class AppButton;
class BrowseField;
class LineEdit;
class ProgressBar;
class Stepper;
class StyledLabel;

/*
 * 注入: DLL path + window keyword configuration, the live step list, a
 * port-wait progress bar and the run controls.
 */
class InjectPage : public Page
{
    Q_OBJECT

public:
    explicit InjectPage(QWidget* parent = nullptr);

    QString dllPath() const;
    QString keyword() const;

    void reloadSettings();

    void onSequenceState(InjectionSequence::State state);
    void onTargetFound(quint32 pid, const QString& title);
    void onWaitPortProgress(int elapsedMs, int totalMs);
    void onFinished(bool ok, bool resumed, const QString& message);

    static bool runningAsAdmin();

signals:
    void startRequested();
    void cancelRequested();
    void restartAsAdminRequested();

private:
    void buildAdminBanner();

    BrowseField* m_dll = nullptr;
    LineEdit* m_keyword = nullptr;

    AppButton* m_start = nullptr;
    AppButton* m_cancel = nullptr;
    Stepper* m_steps = nullptr;
    ProgressBar* m_progress = nullptr;

    StyledLabel* m_resultTitle = nullptr;
    StyledLabel* m_resultDetail = nullptr;
    QWidget* m_resultBanner = nullptr;
};
