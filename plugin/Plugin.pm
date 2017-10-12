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
    'description'  => Slim::Utils::Strings::string('PLUGIN_HUEBRIDGE_NAME'),
});

my $prefs = preferences('plugin.huebridge');


$prefs->init({
    binaryAutorun => 0,
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

        Slim::Web::Pages->addPageFunction("plugins/HueBridge/settings/tableHueBridges.html" => \&Plugins::HueBridge::Settings::handler_tableHueBridges);
        
        Slim::Web::Pages->addPageFunction("plugins/HueBridge/huebridge-log.html" => \&Plugins::HueBridge::Squeeze2Hue::handler_logFile);
        Slim::Web::Pages->addPageFunction(qr/plugins\/HueBridge\/settings\/huebridge-([0-9a-f]{12})-state\.html/, \&Plugins::HueBridge::Squeeze2Hue::handler_stateFile);
        Slim::Web::Pages->addPageFunction("plugins/HueBridge/settings/huebridge-config.html", \&Plugins::HueBridge::Squeeze2Hue::handler_configFile);
        Slim::Web::Pages->addPageFunction("plugins/HueBridge/huebridgeguide.html", \&Plugins::HueBridge::Squeeze2Hue::handler_userGuide);
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
