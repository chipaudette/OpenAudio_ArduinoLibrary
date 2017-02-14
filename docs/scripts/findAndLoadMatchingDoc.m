function all_lines = findAndLoadMatchingDoc(node_name,dir_f32,dir_orig)
all_lines={};

%look first in the f32 directory
fnames = dir([dir_f32 node_name '.html']);
if isempty(fnames); fnames = dir([dir_orig node_name '.html']);end

%maybe this is an f32 version of an original module
modify_html_for_f32=0;
if isempty(fnames);
    if (node_name(end-3:end) == '_F32')
        fnames = dir([dir_orig node_name(1:end-4) '.html']);
        modify_html_for_f32=1;
    end
end
    
if isempty(fnames)
    return;
end

if length(fnames) > 1
    disp(['*** WARNING ***: findAndLoadMatchingDoc: more than one HTML file found for ' node_name]);
    for I=1:length(fnames)
        disp(['    : ' fnames(I).name]);
    end
    disp(['    : Using first one.']);
end

%load the file
all_lines = readAllLines([fnames(1).folder '\' fnames(1).name]);

%adjust as needed
if modify_html_for_f32
    name_as_orig = node_name(1:end-4);
    for Iline=1:length(all_lines)
        line = all_lines{Iline};
        I=strfind(line,name_as_orig);
        if ~isempty(I)
            new_line = [];
            if (I(1) > 1)
                new_line = [new_line line(1:I(1)-1)];
            end
            new_line = [new_line node_name];
            new_line = [new_line line((I(1)+length(name_as_orig)):end)];
            all_lines{Iline}=new_line;
        end
    end
end
            
        

