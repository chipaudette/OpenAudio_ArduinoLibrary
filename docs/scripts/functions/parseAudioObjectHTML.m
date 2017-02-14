function all_docs = parseAudioObjectHTML(fname,outpname);

if nargin < 2
    outpname = 'NodeDocs\';
    if nargin < 1
        fname = 'Temp\node_docs.txt';
    end
end

%% get the data
if iscell(fname)
    % we're already given the text, so no need to load it
    all_lines = fname;
else
    % read file
    fid=fopen(fname,'r');
    all_lines=[];
    tline=fgetl(fid);
    while ischar(tline)
        all_lines{end+1} = tline;
        tline=fgetl(fid);
    end
    fclose(fid);
end

%% parse the file into subfiles
all_data=[];
targ_str = '<script type="text/x-red" data-help-name=';
row_inds = find(contains(all_lines,targ_str));
if isempty(row_inds)
    disp(['*** ERROR ***: parseDocsFile: could not find any node docs.  returning...']);
    return;
end

%add an entry at the end to ensure that the last doc is included
row_inds(end+1) = length(all_lines)+1;

%loop over each doc and save
for Idoc = 1:length(row_inds)-1
    %get indices for this doc and extract
    inds = [row_inds(Idoc) row_inds(Idoc+1)-1];
    node_doc = all_lines(inds(1):inds(2));
    
    %extract the name of the doc
    name_str = node_doc{1};
    targ_str = 'data-help-name=';
    I=strfind(name_str,targ_str);
    name_str = name_str((I(1)+length(targ_str)):end);
    I=find(name_str=='"');
    name_str = name_str((I(1)+1):(I(2)-1));
    
    %write doc
    outfname = [outpname name_str '.html'];
    writeText(outfname,node_doc);
    
    %add to data structure
    data=[];
    data.name = name_str;
    data.doc = node_doc;
    if isempty(all_data)
        all_data = data;
    else
        all_data(end+1) = data;
    end
    
end

all_docs = all_data;

%% %%%%%%%%%%%%%%%%55
function []=writeText(outfname,textToWrite);

disp(['writing text to ' outfname]);
fid=fopen(outfname,'w');
for I=1:length(textToWrite)
    fprintf(fid,'%s\n',textToWrite{I});
end
fclose(fid);
