#!/usr/bin/perl

# Script to illustrate how to parse a simple XML file
# and dump its contents in a Perl hash record.

use strict;
use XML::Simple;
use File::Basename;
use Data::Dumper;
use Digest::SHA1 qw(sha1 sha1_hex);

my $version = $ARGV[3];
my $zip = $ARGV[2];
my $xmlpath = $ARGV[1];
my $xmlfile;
my $xmldata;

use constant SF => 'http://downloads.sourceforge.net/project';

my $sf = { HueBridge => '/lms-to-hue/dev',
	 };

my $zipfile = $xmlpath.'/'.$zip.'-'.$version.'.zip';

$xmlfile = $xmlpath . '/' . 'repo-sf.xml';
		
if ($ARGV[0] eq 'sha') {
	open (my $fh, "<", $zipfile);
	binmode $fh;
	
	my $digest = Digest::SHA1->new;
	$digest->addfile($fh);
	close $fh;
	
	my $sha = $digest->hexdigest;
	my $xmldata;
	
	$xmldata = XMLin($xmlfile, ForceArray => 1, KeepRoot => 0, KeyAttr => '', NoAttr => 0);
	
	$xmldata->{'plugins'}[0]->{'plugin'}[0]->{'sha'}[0] = $sha; 
	print "$sha\n";
			
	XMLout($xmldata, NoSort => 1, RootName => 'extensions', XMLDecl => 1, KeyAttr => '', NoAttr => 0, OutputFile => $xmlfile);
}	

if ($ARGV[0] eq 'version') {

	$xmldata = XMLin($xmlfile, ForceArray => 1, KeepRoot => 0, KeyAttr => '', NoAttr => 0);
		
	$xmldata->{'plugins'}[0]->{'plugin'}[0]->{'version'} = $version;
	$xmldata->{'plugins'}[0]->{'plugin'}[0]->{'url'}[0] = SF . "$sf->{$zip}/$zip-$version.zip";
	print $xmldata->{'plugins'}[0]->{'plugin'}[0]->{'url'}[0];
	
	XMLout($xmldata, NoSort => 1, RootName => 'extensions', XMLDecl => 1, KeyAttr => '', NoAttr => 0, OutputFile => $xmlfile);
	
	my $xmlinstall = $xmlpath.'/plugin/install.xml';
	print "versioning $xmlinstall $version\n";
	my $xmldata = XMLin($xmlinstall, ForceArray => 1, KeepRoot => 0, KeyAttr => '', NoAttr => 0);
	$xmldata->{'version'}[0] = $version;

	XMLout($xmldata, NoSort => 1, RootName => 'extensions', XMLDecl => 1, KeyAttr => '', NoAttr => 0, OutputFile => $xmlinstall);
}	

