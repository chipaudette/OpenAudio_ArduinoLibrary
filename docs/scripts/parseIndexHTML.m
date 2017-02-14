%function parseIndexHTML(fname);

%This routine reads the index.html file and breaks it up into its
%constituent pieces ("ParsedInputs\" for susequent analysis

addpath('functions\');

fname = '..\orig_index.html';

%% read file
all_lines = readAllLines(fname);

%% find the start of the nodes definition
target_str = '<script  type="text/x-red" data-container-name="NodeDefinitions">';
row_inds=find(contains(all_lines,target_str));
if isempty(row_inds)
    disp(['*** ERROR ***: could not find node definitions.  Returning...']);
    return;
end
row_inds = row_inds(1);
header = all_lines(1:row_inds-1);
all_lines = all_lines(row_inds:end);

%save the header
outfname = 'ParsedInputs\header.txt';
writeText(outfname,header);


%% extract the transition to the nodes
row_ind = 3;
textToWrite = all_lines(1:row_ind-1);
all_lines = all_lines(row_ind:end);

%save the transition
outfname = 'ParsedInputs\transition_to_nodes.txt';
writeText(outfname,textToWrite);

%% extract the nodes
target_str = '</script>';
row_inds=find(contains(all_lines,target_str));
if isempty(row_inds)
    disp(['*** ERROR ***: could not find end of nodes.  Returning...']);
    return;
end

row_inds = row_inds(1);
row_inds = row_inds-1;  %back up one to get to the ']}' text instead

textToWrite = all_lines(1:row_inds-1);
all_lines = all_lines(row_inds:end);

%save the transition
outfname = 'ParsedInputs\nodes.txt';
writeText(outfname,textToWrite);

%% extract the transition to docs
target_str = '<script type="text/x-red" data-help-name=';
row_inds=find(contains(all_lines,target_str));
if isempty(row_inds)
    disp(['*** ERROR ***: could not find start of docs.  Returning...']);
    return;
end

row_inds = row_inds(1);

textToWrite = all_lines(1:row_inds-1);
all_lines = all_lines(row_inds:end);

%save the transition
outfname = 'ParsedInputs\transitionToDocs.txt';
writeText(outfname,textToWrite);

%% extract the docs
target_str = '</body>';
row_inds=find(contains(all_lines,target_str));
if isempty(row_inds)
    disp(['*** ERROR ***: could not find end of docs.  Returning...']);
    return;
end

row_inds = row_inds(1);

textToWrite = all_lines(1:row_inds-1);
all_lines = all_lines(row_inds:end);

%save the transition
outfname = 'ParsedInputs\node_docs.txt';
writeText(outfname,textToWrite);

%% parse the docs and write the invidiual docs
all_docs = parseAudioObjectHTML(outfname,'..\audio_html\');

%% extract the end of the file
outfname = 'ParsedInputs\end_of_file.txt';
writeText(outfname,all_lines);

