package Plugins::HueBridge::Plugin;

use strict;

use Data::Dumper;
use File::Spec::Functions;
use XML::Simple;
use base qw(Slim::Plugin::Base);

use Slim::Utils::Prefs;
use Slim::Utils::Log;

my $prefs = preferences('plugin.huebridge');

$prefs->init({ autorun => 0, opts => '', debugs => '', logging => 0, bin => undef, configfile => "huebridge.xml", profilesURL => initProfilesURL(), autosave => 1, eraselog => 0});

my $log = Slim::Utils::Log->addLogCategory({
	'category'     => 'plugin.huebridge',
	'defaultLevel' => 'WARN',
	'description'  => Slim::Utils::Strings::string('PLUGIN_HUEBRIDGE'),
}); 

sub initPlugin {
	my $class = shift;

	$class->SUPER::initPlugin(@_);
		
	require Plugins::HueBridge::Squeeze2hue;		
	
	if ($prefs->get('autorun')) {
		Plugins::HueBridge::Squeeze2hue->start;
	}
	
	if (!$::noweb) {
		require Plugins::HueBridge::Settings;
		Plugins::HueBridge::Settings->new;
		Slim::Web::Pages->addPageFunction("^huebridge-log.log", \&Plugins::HueBridge::Squeeze2hue::logHandler);
		Slim::Web::Pages->addPageFunction("^huebridge-config.xml", \&Plugins::HueBridge::Squeeze2hue::configHandler);
		Slim::Web::Pages->addPageFunction("huebridge-guide.htm", \&Plugins::HueBridge::Squeeze2hue::guideHandler);
	}
	
	$log->warn(Dumper(Slim::Utils::OSDetect::details()));
}

sub initProfilesURL {
	my $file = catdir(Slim::Utils::PluginManager->allPlugins->{'HueBridge'}->{'basedir'}, 'install.xml');
	return XMLin($file, ForceArray => 0, KeepRoot => 0, NoAttr => 0)->{'profilesURL'};
}

sub shutdownPlugin {
	if ($prefs->get('autorun')) {
		Plugins::HueBridge::Squeeze2hue->stop;
	}
}

1;
