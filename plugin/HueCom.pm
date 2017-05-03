package Plugins::HueBridge::HueCom;

use strict;

use Slim::Networking::SimpleAsyncHTTP;
use Slim::Utils::Log;
use Slim::Utils::Prefs;

use JSON::XS;
use XML::Simple;

my $prefs = preferences('plugin.huebridge');
my $log = logger('plugin.huebridge');

my $bridgeUserName = 'none';

my $connectProgress = 0;
my $connectDisconnectStatus = 0;

sub initCLICommands {
    # Command init
    #    |requires client
    #    |  |is a query
    #    |  |  |has tags
    #    |  |  |  |function to call
    #    C  Q  T  F
    
    Slim::Control::Request::addDispatch(['hue','bridge','connect','progress'],
        [0, 1, 0, \&getHueBridgeConnectProgress]);
    
    Slim::Control::Request::addDispatch(['hue','bridge','connect','cancel'],
        [0, 1, 0, \&cancelHueBridgeConnect]);
}

sub getHueBridgeConnectProgress {
    my $request = shift;
    
    $request->addResult('_hueBridgeConnectProgress', sprintf("%.2f", Plugins::HueBridge::HueCom->getConnectProgress()));
    
    $request->setStatusDone();
}

sub cancelHueBridgeConnect {
    my $request = shift;
    
    if(Plugins::HueBridge::HueCom->getConnectDisconnectStatus()) {
        Plugins::HueBridge::HueCom->unconnect();
        $request->addResult('_hueBridgeConnectCancel', 1);
    }
    else {
        $request->addResult('_hueBridgeConnectCancel', 0);
    }
    
    $request->setStatusDone();
}

sub connect {
    my ($self, $bridgeIpAddress) = @_;
    
    $connectProgress = 0;
    $bridgeUserName = 'none';
    
    $log->debug('Initiating hue bridge connect (' . $bridgeIpAddress . ').');
    
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&_sendConnectRequest, $bridgeIpAddress);
    Slim::Utils::Timers::setTimer(undef, time() + 30, \&unconnect);
    
    $connectDisconnectStatus = 1;
}

sub disconnect {
    my ($self, $bridgeIpAddress) = @_;
    
    $connectDisconnectStatus = 1;
    
    $log->debug('Disconnecting device (' . $bridgeIpAddress .').');
    
    $bridgeUserName = 'none';
    
    $connectDisconnectStatus = 0;
}

sub unconnect {
    $log->debug('Stopping HueBridge connect.');
    
    Slim::Utils::Timers::killTimers(undef, \&_sendConnectRequest);
    Slim::Utils::Timers::killTimers(undef, \&unconnect);
    
    $connectProgress = 0;
    $connectDisconnectStatus = 0;
}

sub getBridgeUserName {
    return $bridgeUserName;
}

sub getConnectProgress {
    return ($connectProgress >= 0.0) ? ($connectProgress / 30) : -1;
}

sub getConnectDisconnectStatus {
    return $connectDisconnectStatus;
}

sub _sendConnectRequest {
    my ($self, $bridgeIpAddress) = @_;
    
    my $uri = join('', 'http://', $bridgeIpAddress, '/api');
    my $contentType = "application/json";
    
    my $request = { 'devicetype' => 'squeeze2hue#lms' };
    my $requestBody = encode_json($request);
    
    my $asyncHTTP = Slim::Networking::SimpleAsyncHTTP->new(
        \&_sendConnectRequestOK,
        \&_sendConnectReqeustERROR,
        {
            bridgeIpAddress => $bridgeIpAddress,
            error       => "Can't get response.",
        },
    );
    
    $connectDisconnectStatus = 1;
    $asyncHTTP->_createHTTPRequest(POST => ($uri, 'Content-Type' => $contentType, $requestBody));
}

sub _sendConnectRequestOK{

    my $asyncHTTP = shift;
    my $bridgeIpAddress = $asyncHTTP->params('bridgeIpAddress');
    
    my $content = $asyncHTTP->content();
    
    $log->debug('Connect request sucessfully sent to hue bridge (' . $bridgeIpAddress . ').');
    my $bridgeResponse = decode_json($asyncHTTP->content);
    
   
    if(exists($bridgeResponse->[0]->{error})){
        $log->info('Pairing with hue bridge (' . $bridgeIpAddress . ' failed.');
        $log->debug('Message from hue bridge was: \'' .$bridgeResponse->[0]->{error}->{description}. '\'');
        
        $connectProgress += 1;
        Slim::Utils::Timers::setTimer(undef, time() + 1, \&_sendConnectRequest, $bridgeIpAddress);
    }
    elsif(exists($bridgeResponse->[0]->{success})) {
        $log->debug('Pairing with hue bridge (' . $bridgeIpAddress . ' successful.');
        $log->debug('Got username \'' . $bridgeResponse->[0]->{success}->{username} . '\' from hue bridge (' . $bridgeIpAddress . ').');
        
        $bridgeUserName = $bridgeResponse->[0]->{success}->{username};
        
        unconnect();
    }
    else{
        $log->error('Got nothing useful at all from hue bridge (' . $bridgeIpAddress . ' ).');
        
        $connectProgress += 1;
        Slim::Utils::Timers::setTimer(undef, time() + 1, \&_sendConnectRequest, $bridgeIpAddress);
    }
}

sub _sendConnectRequestERROR {
    my $asyncHTTP = shift;
    my $bridgeIpAddress = $asyncHTTP->params('bridgeIpAddress');
    
    $log->error('Request to hue bridge (' . $bridgeIpAddress . ') could not be processed.');
    
    $connectProgress += 1;
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&connectRequest, $bridgeIpAddress );
}

1;
