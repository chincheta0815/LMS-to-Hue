#! /bin/bash
version=0.0.1.0.1
devroot=.
export type=stable

PERL=/opt/perl-5.18.2/bin/perl
export PERL5LIB=$(pwd)/perl/lib/perl5

${PERL} package.pl version ${devroot} HueBridge ${version%} ${type}
rm ${devroot}/HueBridge*.zip
zip -r ${devroot}/HueBridge-${version}.zip ${devroot}/plugin/*
${PERL} package.pl sha ${devroot} HueBridge ${version} ${type}

chown -R 500:500 .
chmod -R A=owner@::file_inherit/dir_inherit:deny,owner@:full_set:file_inherit/dir_inherit:allow,group@::file_inherit/dir_inherit:deny,group@:full_set:file_inherit/dir_inherit:allow,everyone@:write_set:file_inherit/dir_inherit:deny,everyone@:read_set/execute:file_inherit/dir_inherit:allow .
