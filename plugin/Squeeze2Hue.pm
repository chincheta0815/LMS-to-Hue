package Plugins::HueBridge::Squeeze2Hue;

use strict;

use Proc::Background;
use File::ReadBackwards;
use File::Spec::Functions;
use Data::Dumper;

use Slim::Utils::Log;
use Slim::Utils::Prefs;
use XML::Simple;

my $prefs = preferences('plugin.huebridge');
my $log   = logger('plugin.huebridge');

my $squeeze2hue;
my $binary;

sub getAvailableHelperBinaries {
	my $os = Slim::Utils::OSDetect::details();
	
	if ($os->{'os'} eq 'Linux') {

		if ($os->{'osArch'} =~ /x86_64/) {
			return qw(squeeze2hue-x86-64 squeeze2hue-x86-64-static);
		}
		if ($os->{'binArch'} =~ /i386/) {
			return qw(squeeze2hue-x86 squeeze2hue-x86-static);
		}
		if ($os->{'binArch'} =~ /armhf/) {
			return qw(squeeze2hue-armv6hf squeeze2hue-armv6hf-static);
		}
		if ($os->{'binArch'} =~ /arm/) {
			return qw(squeeze2hue-armv5te squeeze2hue-armv5te-static);
		}
		
		# fallback to offering all linux options for case when architecture detection does not work
		return qw(squeeze2hue-x86-64 squeeze2hue-x86-64-static squeeze2hue-x86 squeeze2hue-x86-static squeeze2hue-armv6hf squeeze2hue-armv6hf-static);
	}
	
	if ($os->{'os'} eq 'Unix') {
	
		if ($os->{'osName'} eq 'solaris') {
			return qw(squeeze2hue-i86pc-solaris squeeze2hue-i86pc-solaris-static);
		}	
		
	}	
	
	if ($os->{'os'} eq 'Darwin') {
		return qw(squeeze2hue-osx-multi squeeze2hue-osx-multi-static);
	}
	
	if ($os->{'os'} eq 'Windows') {
		return qw(squeeze2hue-win.exe);
	}	
	
=comment	
		if ($os->{'isWin6+'} ne '') {
			return qw(squeeze2hue-win.exe);
		} else {
			return qw(squeeze2hue-winxp.exe);
		}	
	}
=cut	
	
}

sub getHelperBinary {
	my @availableHelperBinaries = getAvailableHelperBinaries();

	if (scalar @availableHelperBinaries == 1) {
		return $availableHelperBinaries[0];
	}

	if (my $b = $prefs->get("bin")) {
		for my $helperBinary (@availableHelperBinaries) {
			if ($helperBinary eq $b) {
				return $b;
			}
		}
	}

	return undef;
}

sub start {
	my $helperBinary = Plugins::HueBridge::Squeeze2Hue->getHelperBinary() || do {
		$log->warn("no helper binary set");
		return;
	};

	my @params;
	my $logging;

	push @params, ("-Z");
	
	if ($prefs->get('autosave')) {
		push @params, ("-I");
	}
	
	if ($prefs->get('eraselog')) {
		unlink Plugins::HueBridge::Squeeze2Hue->logFile();
	}
	
	if ($prefs->get('useLMSsocket')) {
		push @params, ("-b", Slim::Utils::Network::serverAddr());
	}

	if ($prefs->get('logging')) {
		push @params, ("-f", Plugins::HueBridge::Squeeze2Hue->logFile() );
		
		if ($prefs->get('debugs') ne '') {
			push @params, ("-d", $prefs->get('debugs') . "=debug");
		}
		$logging = 1;
	}
	
	if (-e Plugins::HueBridge::Squeeze2Hue->configFile() || $prefs->get('autosave')) {
		push @params, ("-x", Plugins::HueBridge::Squeeze2Hue->configFile() );
	}
	
	if ($prefs->get('opts') ne '') {
		push @params, split(/\s+/, $prefs->get('opts'));
	}
	
	if (Slim::Utils::OSDetect::details()->{'os'} ne 'Windows') {
		my $exec = catdir(Slim::Utils::PluginManager->allPlugins->{'HueBridge'}->{'basedir'}, 'Bin', $helperBinary);
		$exec = Slim::Utils::OSDetect::getOS->decodeExternalHelperPath($exec);
			
		if (!((stat($exec))[2] & 0100)) {
			$log->warn('executable not having \'x\' permission, correcting');
			chmod (0555, $exec);
		}	
	}	
	
	my $path = Slim::Utils::Misc::findbin($helperBinary) || do {
		$log->warn("$helperBinary not found");
		return;
	};

	my $path = Slim::Utils::OSDetect::getOS->decodeExternalHelperPath($path);
			
	if (!-e $path) {
		$log->warn("$helperBinary not executable");
		return;
	}
	
	push @params, @_;

	if ($logging) {
		open(my $fh, ">>", Plugins::HueBridge::Squeeze2Hue->logFile() );
		print $fh "\nStarting Squeeze2hue: $path @params\n";
		close $fh;
	}
	
	eval { $squeeze2hue = Proc::Background->new({ 'die_upon_destroy' => 1 }, $path, @params); };

	if ($@) {

		$log->warn($@);

	} else {
		Slim::Utils::Timers::setTimer(undef, Time::HiRes::time() + 1, sub {
			if ($squeeze2hue && $squeeze2hue->alive() ) {
				$log->debug("$helperBinary running");
				$helperBinary = $path;
			}
			else {
				$log->debug("$helperBinary NOT running");
			}
		});
		
		Slim::Utils::Timers::setTimer(undef, Time::HiRes::time() + 30, \&beat, $path, @params);
	}
}

sub beat {
	my ($class, $path, @args) = @_;
	
	if ($prefs->get('autorun') && !($squeeze2hue && $squeeze2hue->alive)) {
		$log->error('crashed ... restarting');
		
		if ($prefs->get('logging')) {
			open(my $fh, ">>", $class->logFile);
			print $fh "\nRetarting Squeeze2hue after crash: $path @args\n";
			close $fh;
		}
		
		eval { $squeeze2hue = Proc::Background->new({ 'die_upon_destroy' => 1 }, $path, @args); };
	}	
	
	Slim::Utils::Timers::setTimer($class, Time::HiRes::time() + 30, \&beat, $path, @args);
}

sub stop {
	my $class = shift;

	if ($squeeze2hue && $squeeze2hue->alive) {
		$log->info("killing squeeze2hue");
		$squeeze2hue->die;
	}
}

sub alive {
	return ($squeeze2hue && $squeeze2hue->alive) ? 1 : 0;
}

sub wait {
	$log->info("waiting for squeeze2hue to end");
	$squeeze2hue->wait
}

sub restart {
	my $class = shift;

	$class->stop;
	$class->start;
}

sub logFile {
	return catdir(Slim::Utils::OSDetect::dirsFor('log'), "huebridge.log");
}

sub configFile {
	return catdir(Slim::Utils::OSDetect::dirsFor('prefs'), $prefs->get('configfile'));
}

sub logHandler {
	my ($client, $params, undef, undef, $response) = @_;

	$response->header("Refresh" => "10; url=" . $params->{path} . ($params->{lines} ? '?lines=' . $params->{lines} : ''));
	$response->header("Content-Type" => "text/plain; charset=utf-8");

	my $body = '';
	my $file = File::ReadBackwards->new(logFile());
	
	if ($file){

		my @lines;
		my $count = $params->{lines} || 1000;

		while ( --$count && (my $line = $file->readline()) ) {
			unshift (@lines, $line);
		}

		$body .= join('', @lines);

		$file->close();			
	};

	return \$body;
}

sub configHandler {
	my ($client, $params, undef, undef, $response) = @_;

	$response->header("Content-Type" => "text/xml; charset=utf-8");

	my $body = '';
	
	$log->error(configFile());
	
	if (-e configFile) {
		open my $fh, '<', configFile;
		
		read $fh, $body, -s $fh;
		close $fh;
	}	

	return \$body;
}

sub guideHandler {
	my ($client, $params) = @_;
		
	return Slim::Web::HTTP::filltemplatefile('plugins/HueBridge/userguide.htm', $params);
}

1;
