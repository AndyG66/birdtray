#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>

#include "version.h"
#include "settings.h"
#include "dialogsettings.h"
#include "modelaccounttree.h"
#include "modelnewemails.h"
#include "databaseaccounts.h"
#include "dialogaddeditaccount.h"
#include "databaseunreadfixer.h"


DialogSettings::DialogSettings( QWidget *parent)
    : QDialog(parent), Ui::DialogSettings()
{
    setupUi(this);
    mProgressFixer = 0;

    // Show the first tab
    tabWidget->setCurrentIndex( 0 );

    connect( buttonBox, &QDialogButtonBox::accepted, this, &DialogSettings::accept );
    connect( buttonBox, &QDialogButtonBox::rejected, this, &DialogSettings::reject );
    connect( btnBrowse, &QPushButton::clicked, this, &DialogSettings::browsePath );
    connect( leProfilePath, &QLineEdit::textChanged, this, &DialogSettings::profilePathChanged );
    connect( btnFixUnreadCount, &QPushButton::clicked, this, &DialogSettings::fixDatabaseUnreads );
    connect( tabWidget, &QTabWidget::currentChanged, this, &DialogSettings::activateTab );

    connect( btnNotificationIcon, &QPushButton::clicked, this, &DialogSettings::buttonChangeIcon );
    connect( btnDefaultIcon, &QPushButton::clicked, this, &DialogSettings::buttonDefaultIcon );

    connect( treeAccounts, &QTreeView::doubleClicked, this, &DialogSettings::accountEditIndex  );
    connect( btnAccountAdd, &QPushButton::clicked, this, &DialogSettings::accountAdd );
    connect( btnAccountEdit, &QPushButton::clicked, this, &DialogSettings::accountEdit );
    connect( btnAccountRemove, &QPushButton::clicked, this, &DialogSettings::accountRemove );

    connect( treeNewEmails, &QTreeView::doubleClicked, this, &DialogSettings::newEmailEditIndex  );
    connect( btnNewEmailAdd, &QPushButton::clicked, this, &DialogSettings::newEmailAdd );
    connect( btnNewEmailEdit, &QPushButton::clicked, this, &DialogSettings::newEmailEdit );
    connect( btnNewEmailDelete, &QPushButton::clicked, this, &DialogSettings::newEmailRemove );

    // Setup parameters
    leProfilePath->setText( pSettings->mThunderbirdFolderPath );
    btnNotificationColor->setColor( pSettings->mNotificationDefaultColor );
    notificationFont->setCurrentFont( pSettings->mNotificationFont );
    notificationFontWeight->setValue( pSettings->mNotificationFontWeight * 2 );
    sliderBlinkingSpeed->setValue( pSettings->mBlinkSpeed );
    boxLaunchThunderbirdAtStart->setChecked( pSettings->mLaunchThunderbird );
    boxShowHideThunderbird->setChecked( pSettings->mShowHideThunderbird );
    boxHideWhenMinimized->setChecked( pSettings->mHideWhenMinimized );
    boxMonitorThunderbirdWindow->setChecked( pSettings->mMonitorThunderbirdWindow );
    boxRestartThunderbird->setChecked( pSettings->mRestartThunderbird );
    leThunderbirdBinary->setText( pSettings->mThunderbirdCmdLine  );
    leThunderbirdWindowMatch->setText( pSettings->mThunderbirdWindowMatch  );
    spinMinimumFontSize->setValue( pSettings->mNotificationMinimumFontSize );
    spinMinimumFontSize->setMaximum( pSettings->mNotificationMaximumFontSize - 1 );
    boxHideWindowAtStart->setChecked( pSettings->mHideWhenStarted );
    boxHideWindowAtRestart->setChecked( pSettings->mHideWhenRestarted );
    boxEnableNewEmail->setChecked( pSettings->mNewEmailMenuEnabled );

    if ( pSettings->mLaunchThunderbird )
        boxStopThunderbirdOnExit->setChecked( pSettings->mExitThunderbirdWhenQuit );

    // Prepare the error palette
    mPaletteErrror = mPaletteOk = leProfilePath->palette();
    mPaletteErrror.setColor( QPalette::Text, Qt::red );

    // Accounts tab
    mAccountModel = new ModelAccountTree( this );
    treeAccounts->setModel( mAccountModel );

    // New emails tab
    mModelNewEmails = new ModelNewEmails( this );
    treeNewEmails->setModel( mModelNewEmails );

    // Create the "About" box
    browserAbout->setText( tr("<html>This is Birdtray version %1.%2<br>Copyright (C) George Yunaev, gyunaev@ulduzsoft.com 2018<br>Licensed under GPLv3 or higher</html>") .arg( VERSION_MAJOR ) .arg( VERSION_MINOR) );

    // Icon
    btnNotificationIcon->setIcon( pSettings->mNotificationIcon );

    // Parsers
    boxParserSelection->addItem( tr("sqlite (default)"), false );
    boxParserSelection->addItem( tr("mork parser (experimental)"), true );

    if ( pSettings->mUseMorkParser )
    {
        boxParserSelection->setCurrentIndex( 1 );

        // Fix Unread is useless in this mode
        btnFixUnreadCount->setEnabled( false );
    }
    else
        boxParserSelection->setCurrentIndex( 0 );

    profilePathChanged();
}

void DialogSettings::accept()
{
    // Validate the profile path
    QString profilePath = leProfilePath->text();

    if ( profilePath.isEmpty() )
    {
        QMessageBox::critical( 0, "Empty Thunderbird directory", tr("You must specify Thunderbird directory") );
        return;
    }

    if ( boxEnableNewEmail->isChecked() && leThunderbirdBinary->text().isEmpty() )
    {
        QMessageBox::critical( 0, "Empty Thunderbird path", tr("You have enabled New Email menu, but you did not specify Thunderbird path") );
        tabWidget->setCurrentIndex( 0 );
        leThunderbirdBinary->setFocus();
        return;
    }

    if ( !profilePath.endsWith( QDir::separator() ) )
        profilePath.append( QDir::separator() );

    if ( !QFile::exists( profilePath + "global-messages-db.sqlite" ) )
    {
        QMessageBox::critical( 0, "Invalid Thunderbird directory", tr("Valid Thunderbird directory must contain the file global-messages-db.sqlite") );
        return;
    }

    bool use_mork = boxParserSelection->currentIndex() == 1;
    bool delete_accounts = false;

    if (  use_mork != pSettings->mUseMorkParser )
    {
        if ( QMessageBox::question( 0,
                               tr("WARNING: PARSER CHANGED"),
                               tr("You have changed the parser. This will delete all accounts in the Accounts tab, and you must restart Birdtray and rea-add them.\n\nReally change the parser?") )
                != QMessageBox::Yes )
                return;

        pSettings->mUseMorkParser = use_mork;
        delete_accounts = true;
    }

    pSettings->mThunderbirdFolderPath = leProfilePath->text();
    pSettings->mNotificationDefaultColor = btnNotificationColor->color();
    pSettings->mNotificationFont = notificationFont->currentFont();
    pSettings->mBlinkSpeed = sliderBlinkingSpeed->value();
    pSettings->mLaunchThunderbird = boxLaunchThunderbirdAtStart->isChecked();
    pSettings->mShowHideThunderbird = boxShowHideThunderbird->isChecked();
    pSettings->mThunderbirdCmdLine = leThunderbirdBinary->text();
    pSettings->mThunderbirdWindowMatch = leThunderbirdWindowMatch->text();
    pSettings->mHideWhenMinimized = boxHideWhenMinimized->isChecked();
    pSettings->mNotificationFontWeight = notificationFontWeight->value() / 2;
    pSettings->mExitThunderbirdWhenQuit = boxStopThunderbirdOnExit->isChecked();

    pSettings->mMonitorThunderbirdWindow = boxMonitorThunderbirdWindow->isChecked();
    pSettings->mNotificationIcon = btnNotificationIcon->icon().pixmap( pSettings->mIconSize );
    pSettings->mNotificationMinimumFontSize = spinMinimumFontSize->value();
    pSettings->mRestartThunderbird = boxRestartThunderbird->isChecked();
    pSettings->mHideWhenStarted = boxHideWindowAtStart->isChecked();
    pSettings->mHideWhenRestarted = boxHideWindowAtRestart->isChecked();
    pSettings->mNewEmailMenuEnabled = boxEnableNewEmail->isChecked();

    mModelNewEmails->applySettings();

    if ( delete_accounts )
    {
        pSettings->mFolderNotificationColors.clear();
        QApplication::quit();
    }
    else
        mAccountModel->applySettings();

    QDialog::accept();
}

void DialogSettings::browsePath()
{
    QString e = QFileDialog::getExistingDirectory( 0,
                                                   "Choose the Thunderbird profile path",
                                                   leProfilePath->text(),
                                                   QFileDialog::ShowDirsOnly );

    if ( e.isEmpty() )
        return;

    if ( !e.endsWith( QDir::separator() ) )
        e.append( QDir::separator() );

    if ( !QFile::exists( e + "global-messages-db.sqlite" ) )
    {
        QMessageBox::critical( 0, "Invalid Thunderbird directory", tr("Valid Thunderbird directory must contain the file global-messages-db.sqlite") );
        return;
    }

    leProfilePath->setText( e );
}


void DialogSettings::profilePathChanged()
{
    bool valid = isProfilePathValid();

    tabAccounts->setEnabled( valid );
    btnFixUnreadCount->setEnabled( valid );

    if ( valid )
        leProfilePath->setPalette( mPaletteOk );
    else
        leProfilePath->setPalette( mPaletteErrror );

}

void DialogSettings::fixDatabaseUnreads()
{
    if ( QMessageBox::question( 0,
                           tr("Fix the unread messages?"),
                           tr("<html>This option should be used if you have no unread messages, but still see the new email counter.<br>"
                              "To use this option, it is mandatory to shut down Thunderbird.<br>"
                              "Fixing may take up to five minutes.<br><br>"
                              "Please confirm that Thunderbird is shut down, and you want to proceed?</html>") ) ==  QMessageBox::No )
        return;

    mProgressFixer = new QProgressDialog();
    mProgressFixer->setLabelText( tr("Updating the database...") );
    mProgressFixer->setCancelButton( 0 );
    mProgressFixer->setMinimum( 0 );
    mProgressFixer->setMaximum( 100 );
    mProgressFixer->setWindowModality(Qt::WindowModal);
    mProgressFixer->show();

    DatabaseUnreadFixer * fixer = new DatabaseUnreadFixer( leProfilePath->text() );
    connect( fixer, &DatabaseUnreadFixer::done, this, &DialogSettings::databaseUnreadsFixed );
    connect( fixer, &DatabaseUnreadFixer::progress, this, &DialogSettings::databaseUnreadsUpdate );

    fixer->start();
}

void DialogSettings::databaseUnreadsUpdate(int progresspercentage)
{
    mProgressFixer->setValue( progresspercentage );
}

void DialogSettings::databaseUnreadsFixed( QString errorMsg )
{
    qDebug("Done updating the database, error: '%s'", qPrintable( errorMsg ) );

    mProgressFixer->close();
    delete mProgressFixer;
    mProgressFixer = 0;

    if ( errorMsg.isEmpty() )
        QMessageBox::information( 0, "Database updated", "Successfully updated the database");
    else
        QMessageBox::critical( 0, "Error updating database", tr("Error updating the database:\n%1") .arg( errorMsg ));
}

void DialogSettings::accountsAvailable( QString errorMsg )
{
    if ( !errorMsg.isEmpty() )
    {
        QMessageBox::critical( 0, "Error retrieving accounts", tr("Error retrieving accounts:\n%1") .arg( errorMsg ));
        return;
    }

    DatabaseAccounts * dba = (DatabaseAccounts *) sender();
    mAccounts = dba->accounts();
}

void DialogSettings::accountAdd()
{
    DialogAddEditAccount dlg;
    dlg.setCurrent( mAccounts, "", btnNotificationColor->color() );

    if ( dlg.exec() == QDialog::Accepted )
        mAccountModel->addAccount( dlg.account(), dlg.color() );
}

void DialogSettings::accountEdit()
{
    accountEditIndex( treeAccounts->currentIndex() );
}

void DialogSettings::accountEditIndex(const QModelIndex &index)
{
    if ( !index.isValid() )
        return;

    QString uri;
    QColor color;

    mAccountModel->getAccount( index, uri, color );

    DialogAddEditAccount dlg;
    dlg.setCurrent( mAccounts, uri, color );

    if ( dlg.exec() == QDialog::Accepted )
        mAccountModel->editAccount( index, dlg.account(), dlg.color() );
}

void DialogSettings::accountRemove()
{
    QModelIndex index = treeAccounts->currentIndex();

    if ( !index.isValid() )
        return;

    mAccountModel->removeAccount( index );
}

void DialogSettings::newEmailAdd()
{
    mModelNewEmails->add();
}

void DialogSettings::newEmailEdit()
{
    newEmailEditIndex( treeNewEmails->currentIndex() );
}

void DialogSettings::newEmailEditIndex(const QModelIndex &index)
{
    mModelNewEmails->edit( index );
}

void DialogSettings::newEmailRemove()
{
    mModelNewEmails->remove( treeNewEmails->currentIndex() );
}

void DialogSettings::buttonChangeIcon()
{
    QString e = QFileDialog::getOpenFileName( 0,
                                              "Choose the new icon",
                                              "",
                                              "Images(*.png)" );

    if ( e.isEmpty() )
        return;

    QPixmap test;

    if ( !test.load( e ) )
    {
        QMessageBox::critical( 0, "Invalid icon", tr("Could not load the icon from this file") );
        return;
    }

    btnNotificationIcon->setIcon( test.scaled( pSettings->mIconSize ) );
}

void DialogSettings::buttonDefaultIcon()
{
    QPixmap temp;

    if ( temp.load( ":res/thunderbird.png" ) )
        btnNotificationIcon->setIcon( temp.scaled( pSettings->mIconSize ) );
}

void DialogSettings::activateTab(int tab)
{
    // #1 is the Accounts tab
    if ( tab == 1 )
    {
        // Get the account list
        DatabaseAccounts * dba = new DatabaseAccounts( leProfilePath->text() );
        connect( dba, &DatabaseAccounts::done, this, &DialogSettings::accountsAvailable );
        dba->start();
    }
}

bool DialogSettings::isProfilePathValid()
{
    // Validate the profile path
    QString profilePath = leProfilePath->text();

    if ( profilePath.isEmpty() )
        return false;

    if ( !profilePath.endsWith( QDir::separator() ) )
        profilePath.append( QDir::separator() );

    return QFile::exists( profilePath + "global-messages-db.sqlite" );
}
