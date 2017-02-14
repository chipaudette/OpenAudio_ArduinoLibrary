
addpath('functions\');

overall_outfname = '..\index.html';
inpname = 'ParsedInputs\';

all_lines={};

% copy header information from original index.html
infname = [inpname 'header.txt'];
disp(['Copying lines from ' infname]);
foo_lines = readAllLines(infname);
all_lines(end+[1:length(foo_lines)]) = foo_lines;

% copy transition to nodes from original index.html
infname = [inpname 'transition_to_nodes.txt'];
disp(['Copying lines from ' infname]);
foo_lines = readAllLines(infname);  %read the text in
all_lines(end+[1:length(foo_lines)]) = foo_lines; %accumulate the lines

% create new nodes
origNode_fname = 'ParsedInputs\nodes.txt';
newNode_pname = 'C:\Users\wea\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary\';
[nodes] = generateNodes(origNode_fname,newNode_pname);
outfname = 'NewOutputs\new_nodes.txt';
writeNodeText(nodes,outfname);  %write to text file
infname = outfname;
foo_lines = readAllLines(infname); %load the text back in
all_lines(end+[1:length(foo_lines)]) = foo_lines; %accumulate the lines

% copy the transition to docs
infname = [inpname 'transitionToDocs.txt'];
disp(['Copying lines from ' infname]);
foo_lines = readAllLines(infname);  %read the text in
all_lines(end+[1:length(foo_lines)]) = foo_lines; %accumulate the lines

% assemble the docs
for Inode = 1:length(nodes)
    dir_f32 = '..\audio_f32_html\';
    dir_orig = '..\audio_html\';
    foo_lines = findAndLoadMatchingDoc(nodes(Inode).type,dir_f32,dir_orig);
    
    if isempty(foo_lines)
        foo_lines = createEmptyDoc(nodes(Inode).type);
    end
    all_lines(end+[1:length(foo_lines)]) = foo_lines;
end

% copy the end of the file
infname = [inpname 'end_of_file.txt'];
disp(['Copying lines from ' infname]);
foo_lines = readAllLines(infname);  %read the text in
all_lines(end+[1:length(foo_lines)]) = foo_lines; %accumulate the lines


% write the text to the file
disp(['writing main output to ' overall_outfname]);
writeText(overall_outfname,all_lines);



