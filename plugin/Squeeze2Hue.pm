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
my $squeeze2hueRestartStatus = 0;
my $squeeze2hueRestartCounter = 0;
my $squeeze2hueRestartCounterTimeWait = 15;

sub initCLICommands {
    # Command init
    #    |requires client
    #    |  |is a query
    #    |  |  |has tags
    #    |  |  |  |function to call
    #    C  Q  T  F
    
    Slim::Control::Request::addDispatch(['hue', 'bridge', 'restart','?'],
        [0, 1, 0, \&CLI_commandGetSqueeze2hueRestartProgress]);
}

sub CLI_commandGetSqueeze2hueRestartProgress {
    my $request = shift;
    
    $request->addResult('_hueBridgeRestartProgress', sprintf("%.2f", getRestartProgress()));
    
    $request->setStatusDone();
}

sub getAvailableBinaries {
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
		}
		else {
		
			return qw(squeeze2hue-winxp.exe);
		}	
	}
=cut	
	
}

sub getBinary {
	my @availableBinaries = getAvailableBinaries();

	if (scalar @availableBinaries == 1) {
	
		return $availableBinaries[0];
	}

	if (my $b = $prefs->get('binary')) {
	
		for my $binary (@availableBinaries) {
		
			if ($binary eq $b) {
			
				return $b;
			}
		}
	}

	return undef;
}

sub start {
	my $binary = Plugins::HueBridge::Squeeze2Hue->getBinary() || do {
		$log->warn('Squeeze2Hue binary not set.');
		return;
	};

	my @params;
	my $loggingEnabled;

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

	if ($prefs->get('loggingEnabled')) {
	
		push @params, ("-f", Plugins::HueBridge::Squeeze2Hue->logFile() );
		
		if ($prefs->get('debugs') ne '') {
		
			push @params, ("-d", $prefs->get('debugs') . "=debug");
		}
		$loggingEnabled = 1;
	}
	
	if (-e Plugins::HueBridge::Squeeze2Hue->configFile() || $prefs->get('autosave')) {
	
		push @params, ("-x", Plugins::HueBridge::Squeeze2Hue->configFile() );
	}
	
	if ($prefs->get('opts') ne '') {
	
		push @params, split(/\s+/, $prefs->get('opts'));
	}
	
	if (Slim::Utils::OSDetect::details()->{'os'} ne 'Windows') {
	
		my $exec = catdir(Slim::Utils::PluginManager->allPlugins->{'HueBridge'}->{'basedir'}, 'Bin', $binary);
		$exec = Slim::Utils::OSDetect::getOS->decodeExternalHelperPath($exec);
			
		if (!((stat($exec))[2] & 0100)) {
		
			$log->warn('Squeeze2Hue binary not having \'x\' permission, correcting.');
			chmod (0555, $exec);
		}	
	}	
	
	my $path = Slim::Utils::Misc::findbin($binary) || do {
		$log->warn($binary. ' not found.');
		return;
	};

	my $path = Slim::Utils::OSDetect::getOS->decodeExternalHelperPath($path);
			
	if (!-e $path) {
	
		$log->warn($binary. ' not executable.');
		return;
	}
	
	push @params, @_;

	if ($loggingEnabled) {
	
		open(my $fileHandle, ">>", Plugins::HueBridge::Squeeze2Hue->logFile() );
		print($fileHandle, '\nStarting Squeeze2Hue binary (' .$path. ' ' .@params. ')\n');
		close($fileHandle);
	}
	
	eval { $squeeze2hue = Proc::Background->new({ 'die_upon_destroy' => 1 }, $path, @params); };

	if ($@) {

		$log->warn($@);

	}
	else {
	
		Slim::Utils::Timers::setTimer(undef, Time::HiRes::time() + 1, sub {
			if ($squeeze2hue && $squeeze2hue->alive() ) {
				$log->debug($binary. ' running.');
				$binary = $path;
			}
			else {
				$log->debug($binary. ' not running.');
			}
		});
		
		Slim::Utils::Timers::setTimer(undef, Time::HiRes::time() + 30, \&beat, $path, @params);
	}
}

sub beat {
	my ($path, @args) = @_;
	
	if ($prefs->get('autorun') && !($squeeze2hue && $squeeze2hue->alive)) {
	
		$log->error('crashed ... restarting');
		
		if ($prefs->get('loggingEnabled')) {
		
			open(my $fileHandle, ">>", Plugins::HueBridge::Squeeze2Hue->logFile());
			print($fileHandle, '\nSqueeze2Hue binary crashed (' .$path. ' ' .@args. ') ... restarting.\n');
			close($fileHandle);
		}
		
		eval { $squeeze2hue = Proc::Background->new({ 'die_upon_destroy' => 1 }, $path, @args); };
	}	
	
	Slim::Utils::Timers::setTimer(undef, Time::HiRes::time() + 30, \&beat, $path, @args);
}

sub stop {

	if ($squeeze2hue && $squeeze2hue->alive()) {
	
		$log->info("Stopping Squeeze2Hue binary.");
		$squeeze2hue->die;
	}
}

sub alive {
	return ($squeeze2hue && $squeeze2hue->alive) ? 1 : 0;
}

sub wait {
	$log->info("Waiting for Squeeze2Hue binary to end");
	$squeeze2hue->wait
}

sub restart {
    my ($self, $XMLConfig) = @_;
    
    $log->debug('Squeeze2Hue XMLConfig generation requested.');
    
    Plugins::HueBridge::HueCom->blockProgressCounter('block');
    Plugins::HueBridge::Squeeze2Hue->stop();
    $squeeze2hueRestartCounter = 0;
        
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&_performRestart,
                                                    {
                                                            XMLConfig => $XMLConfig,
                                                    },
                                                );

    Slim::Utils::Timers::setTimer(undef, time() + $squeeze2hueRestartCounterTimeWait, \&unrestart);

    $squeeze2hueRestartStatus = 1;
}

sub _performRestart {
    my ($self, $params) = @_;
    my $XMLConfig = $params->{'XMLConfig'};

    $squeeze2hueRestartCounter++;
    
    if ( $squeeze2hueRestartCounter == POSIX::floor($squeeze2hueRestartCounterTimeWait * .3) ) {
    
        if ( $XMLConfig ) {
            $log->debug('Squeeze2Hue XMLConfig writing data to file (' . Plugins::HueBridge::Squeeze2Hue->configFile() . ').');
            writeXMLConfigFile($XMLConfig);
        }
    }
    elsif ( $squeeze2hueRestartCounter == POSIX::floor($squeeze2hueRestartCounterTimeWait * .7) ) {
        
        if ( $XMLConfig ) {
            
            $log->debug('Squeeze2Hue restarting after changed config file.');
            Plugins::HueBridge::Squeeze2Hue->start();
        }
        elsif ( $XMLConfig == undef ) {
            
            $log->debug('Squeeze2Hue generating new config file.');
            Plugins::HueBridge::Squeeze2Hue->start("-i", Plugins::HueBridge::Squeeze2Hue->configFile() );
        }
    }
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&_performRestart,
                                                    {
                                                            XMLConfig => $XMLConfig,
                                                    },
                                                );
}

sub unrestart{
    $log->debug('Terminating Squeeze2Hue XMLConfig restart.');
    
    Slim::Utils::Timers::killTimers(undef, \&_performRestart);
    Slim::Utils::Timers::killTimers(undef, \&unrestart);

    $squeeze2hueRestartCounter = 0;
    Plugins::HueBridge::HueCom->blockProgressCounter('release');
    $squeeze2hueRestartStatus = 0;
}

sub getRestartProgress {
    my $returnValue;

    if ( $squeeze2hueRestartCounter >= 0.0 ) {

        $returnValue =  $squeeze2hueRestartCounter / $squeeze2hueRestartCounterTimeWait;
    }
    else {

        $returnValue = $squeeze2hueRestartCounter;
    }

    return $returnValue;
}

sub blockProgressCounter {
    my $self = shift;
    my $restartCounterState = shift;
    
    if ( $restartCounterState == 'block' ) {
    
        $squeeze2hueRestartCounter = -1;
    }
    elsif ( $restartCounterState == 'release' ) {
    
        $squeeze2hueRestartCounter = 0;
    }
}

sub getRestartStatus {
    return $squeeze2hueRestartStatus;
}

sub logFile {
	return catdir(Slim::Utils::OSDetect::dirsFor('log'), "huebridge.log");
}

sub configFile {
	return catdir(Slim::Utils::OSDetect::dirsFor('prefs'), $prefs->get('configFileName'));
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

		close($file);
	};

	return \$body;
}

sub configHandler {
	my ($client, $params, undef, undef, $response) = @_;

	$response->header("Content-Type" => "text/xml; charset=utf-8");

	my $body = '';
	
	$log->error(configFile());
	
	if (-e configFile) {
	
		open(my $fileHandle, '<', configFile);
		
		read($fileHandle, $body, -s $fileHandle);
		$fileHandle->close();
	}	

	return \$body;
}

sub guideHandler {
	my ($client, $params) = @_;
		
	return Slim::Web::HTTP::filltemplatefile('plugins/HueBridge/userguide.htm', $params);
}

1;
