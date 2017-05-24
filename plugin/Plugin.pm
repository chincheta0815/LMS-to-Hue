package Plugins::HueBridge::Plugin;

use strict;
use base qw(Slim::Plugin::OPMLBased);

use Config;
use File::Spec::Functions;
use XML::Simple;

use Slim::Utils::Log;
use Slim::Utils::Prefs;


my $log = Slim::Utils::Log->addLogCategory({
    'category'     => 'plugin.huebridge',
    'defaultLevel' => 'WARN',
    'description'  => Slim::Utils::Strings::string('PLUGIN_HUEBRIDGE'),
});

my $prefs = preferences('plugin.huebridge');


$prefs->init({
    binaryAutorun => 0,
    showAdvancedHueBridgeOptions => 1,
    opts => '',
    debugs => '',
    loggingEnabled => 0,
    binary => undef,
    configFileName => 'huebridge.xml',
    profilesURL => initProfilesURL(),
    autosave => 1,
    eraselog => 0
});


sub initPlugin {
    my $class = shift;

    $log->debug( "Initialising HueBridge " . $class->_pluginDataFor( 'version' ) . " on " . $Config{'archname'} );

    if ( main::WEBUI ) {

        require Plugins::HueBridge::Settings;
        Plugins::HueBridge::Settings->new;

        Slim::Web::Pages->addPageFunction("tableAdvancedHueBridgeOptions.html" => \&Plugins::HueBridge::Settings::handler_tableAdvancedHueBridgeOptions);
        Slim::Web::Pages->addPageFunction("tableHueBridges.html" => \&Plugins::HueBridge::Settings::handler_tableHueBridges);

        Slim::Web::Pages->addPageFunction("^huebridge.log", \&Plugins::HueBridge::Squeeze2Hue::logHandler);
        Slim::Web::Pages->addPageFunction("^huebridge.xml", \&Plugins::HueBridge::Squeeze2Hue::configHandler);
        Slim::Web::Pages->addPageFunction("huebridgeguide.htm", \&Plugins::HueBridge::Squeeze2Hue::guideHandler);
    }

    require Plugins::HueBridge::HueCom;
    Plugins::HueBridge::HueCom->initCLICommands;

    require Plugins::HueBridge::Squeeze2Hue;
    Plugins::HueBridge::Squeeze2Hue->initCLICommands;
    if ($prefs->get('binaryAutorun')) {

        Plugins::HueBridge::Squeeze2Hue->start;
    }

    return 1;
}

sub initProfilesURL {
    my $file = catdir(Slim::Utils::PluginManager->allPlugins->{'HueBridge'}->{'basedir'}, 'install.xml');

    return XMLin($file, ForceArray => 0, KeepRoot => 0, NoAttr => 0)->{'profilesURL'};
}

sub shutdownPlugin {
    if ($prefs->get('autorun')) {

        Plugins::HueBridge::Squeeze2Hue->stop;
    }
}

1;
