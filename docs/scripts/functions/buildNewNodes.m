function [headings,new_node_data]=buildNewNodes(source_pname)

%Look into directory of objects and build node info from the contents

if nargin < 1
    source_pname = 'C:\Users\wea\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary\';
end

%get all header files
fnames = dir([source_pname '*.h']);

%now look into each header file and find all class definitions
all_class_names={};
all_num_inputs=[];
all_class_lines={};
for Ifile=1:length(fnames)
    [class_names,class_lines] = findClassNames(fnames(Ifile));
    all_class_names(end+[1:length(class_names)]) = class_names;
    %all_num_inputs(end+[1:length(class_names)]) = num_class_inputs;
    all_class_lines(end+[1:length(class_lines)]) = class_lines;
end

%guess a shortname for each one
all_short_names = guessShortName(all_class_names);

%read (or estimate) the number of inputs and outputs
[all_num_inputs, all_num_outputs]= getNumberOfInputsOutputs(all_class_names,all_class_lines);

%guess the number of outputs
%all_num_outputs = guessNumOutputs(all_class_names,all_num_inputs);

%guess the category
all_categories = guessCategory(all_class_names);

%choose icon based on categories
all_icons = chooseIcon(all_class_names,all_categories);

%display results
headings = {'type';'shortName';'inputs';'outputs';'category';'color';'icon'};new_node_data={};
for Iclass=1:length(all_class_names)
    new_node_data{Iclass,1} = all_class_names{Iclass};
    new_node_data{Iclass,2} = all_short_names{Iclass};
    new_node_data{Iclass,3} = all_num_inputs(Iclass);
    new_node_data{Iclass,4} = all_num_outputs(Iclass);
    new_node_data{Iclass,5} = all_categories{Iclass};
    new_node_data{Iclass,6} = '#E6E0F8'; %default color
    new_node_data{Iclass,7} = all_icons{Iclass}; %default icon
    
    str=[];
    for I=1:length(new_node_data(Iclass,:))
        val = new_node_data{Iclass,I};
        if isnumeric(val)
            str = [str num2str(val)];
        else
            str = [str val];
        end
        if I<length(new_node_data(Iclass,:))
            str = [str ','];
        end
    end
    disp(str);
end

if nargout == 0
    disp(' ');
    disp(['Copy the text above into a spreadsheet (like myNodes.xlsx)']);
end


end



%% %%%%%%%%%%%%%%%%%%555
function [names, class_lines] = findClassNames(fname)
if isstruct(fname)
    %assume it is output from Matlab's "dir"
    fname = [fname.folder '\' fname.name];
end

%load the file
all_lines = readAllLines(fname);

%find all of the class names
targ_str = 'class ';
names = {};
row_ind_class = [];
for Iline = 1:length(all_lines)
    line = all_lines{Iline};
    if length(line) >= length(targ_str)
        if strcmpi(line(1:length(targ_str)),targ_str)
            %this is a "class" statment
            
            %strip off the "class" part
            I=strfind(line,targ_str);
            line = line((I(1)+length(targ_str)):end);
            
            %find the end of the name, white or ':'
            I=find((line == ' ') | (line == ':'));
            if isempty(I);  I=length(line); end
            name = line(1:I(1));
            
            %strip off leading/trailing white space
            while (name(1) == ' ') %strip off leading white space
                name = name(2:end);
            end
            while (name(end) == ' ')  %strip off trailing white space
                name = name(1:end-1);
            end
            while (name(end) == ';')  %strip off trailing semicolon
                name = name(1:end-1);
            end
            while (name(end) == ':')  %strip off trailing colon
                name = name(1:end-1);
            end
            
            %skip if class itself is AudioStream_F32 or AudioConnection_F32
            if (strcmpi(name,'AudioStream_F32') | strcmpi(name,'AudioConnection_F32'))
                %ignore
            else
                %save the class name
                names{end+1} = name;
                row_ind_class(end+1) = Iline;
            end
        end
    end
end

%find the constructor for each class to find the number of inputs
class_lines={};
num_class_inputs=zeros(length(row_ind_class),1);
for Iclass = 1:length(row_ind_class)
    %extract the lines just for this class
    line_inds = row_ind_class(Iclass);
    if (Iclass < length(row_ind_class))
        line_inds(2) = row_ind_class(Iclass+1);
    else
        line_inds(2) = length(all_lines);
    end
    class_lines{Iclass} = all_lines(line_inds(1):line_inds(2));
end

end %end function

%% %%%%%%%%%%%%%%%%%%% fu
function [all_num_inputs, all_num_outputs]= getNumberOfInputsOutputs(all_class_names,all_class_lines)

%first, look for comment that is a directive to the "GUI" about number of ins and outs
all_num_inputs = NaN*ones(length(all_class_names),1);
all_num_outputs = NaN*ones(length(all_class_names),1);
for Iclass = 1:length(all_class_names)
    lines = all_class_lines{Iclass};
    targ_str = '//GUI:';
    GUI_lines = find(contains(lines,targ_str));
    
    do_test = 1;
    for Iline=1:length(GUI_lines)
        if do_test
            line = lines{GUI_lines(Iline)};
            targ_str = 'inputs:';
            I=strfind(line,targ_str);
            if ~isempty(I)
                line = line((I(1)+length(targ_str)):end);
                I = strfind(line,',');
                all_num_inputs(Iclass) = str2num(line(1:I(1)-1));
                line = line(I(1)+1:end);
                
                targ_str = 'outputs:';
                I=strfind(line,targ_str);
                if ~isempty(I)
                    line = line((I(1)+length(targ_str)):end);
                    I = find((line == ',') | (line == '/'));
                    line = line(1:I(1)-1);
                    all_num_outputs(Iclass) = str2num(line);
                    do_test = 0;  %we've got our answer.
                end
            end
        end
    end
end
        
%now, do the old method if an answer is missing...look at constructor for inputs
for Iclass = 1:length(all_class_names)
    if isnan(all_num_inputs(Iclass))
        class_lines = all_class_lines{Iclass};
        
        %find the constructor
        targ_str = all_class_names{Iclass};
        I=find(contains(class_lines,targ_str));
        if length(I) < 2
            %disp(['*** WARNING ***: buildNewNodes: could not find constructor for ' names{Iclass}]);
            %disp(['    : Number of audio inputs is unknown.  Continuing...']);
        else
            constuctor_str = class_lines{I(2)};  %constructor should be first one after the name of the class
            
            %find the number of inputs...after AudioStream or AudioStream_F32
            targ_str = 'AudioStream_F32(';
            I=strfind(constuctor_str,targ_str);
            if isempty(I)
                targ_str = 'AudioStream(';
                I=strfind(constuctor_str,targ_str);
            end
            if isempty(I)
                %disp(['*** WARNING ***: buildNewNodes: could not find inputs for ' names{Iclass}]);
                %disp(['    : Number of audio inputs is unknown.  Continuing...']);
            else
                str = constuctor_str((I(1)+length(targ_str)):end);
                I=find(str==',');
                all_num_inputs(Iclass) = str2num(str(1:I(1)-1));
       
            end
        end
    end
end

%look for missing outputs, set equal to inputs
for Iclass = 1:length(all_class_names)
    if isnan(all_num_outputs(Iclass))
        all_num_outputs(Iclass) = all_num_inputs(Iclass);
    end
end

end %end function

%% %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%55
function short_names = guessShortName(class_names)

short_names={};
for Iname=1:length(class_names)
    name = class_names{Iname};
    if strcmpi(name(1:5),'Audio')
        name = name(6:end);
    end
    if strcmpi(name(end-3:end),'_F32')
        name = name(1:end-4);
    end
    
    %strip off known pre-fixes
    strs = {'Control' 'Convert' 'Effect' 'Filter' 'Synth'};
    for Istr=1:length(strs)
        str = strs{Istr};
        if length(name) >= length(str)
            if strcmpi(name(1:length(str)),str)
                name = name((length(str)+1):end);
            end
        end
    end
    
    %handle special cases
    targ_str='Waveform'; %remove "Waveform" from "WaveformSine"
    if (length(name) > length(targ_str))
        if strcmpi(name(1:length(targ_str)),targ_str)
            name = name((length(targ_str)+1):end);
        end
    end
    if strcmpi(name,'sgtl5000_extended')
        name = 'sgtl5000ext';
    end
    if strcmpi(name,'inputI2S')
        name = 'i2sAudioIn';
    end
    if strcmpi(name,'outputI2S')
        name = 'i2sAudioOut';
    end
    
    %strop off leading space or underscore
    while( (name(1) == ' ') | (name(1) == '_')); name=name(2:end);  end
    
    %strop off trailing space or underscore
    while( (name(end) == ' ') | (name(end) == '_')); name=name(1:end-1);  end
    
    %adjust the case
    if strcmpi(name(1:3),'I16') | strcmpi(name(1:3),'F32');
        %don't change the case
    else
        %do change the case
        name(1) = lower(name(1)); %remove any leading capital letter
        if strcmpi(name(1:3),'SGT') | strcmpi(name(1:3),'TLV') | strcmpi(name(1:3),'FIR') | strcmpi(name(1:3),'IIR')
            name=lower(name);  %go all lower case;
        end
    end
    
    %save the name
    short_names{Iname} = name;
end

end %end function


%% %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function num_outputs = guessNumOutputs(class_names,num_inputs)

num_outputs = num_inputs;  %default rule

% apply corrections
for Iclass=1:length(class_names)
    name = class_names{Iclass};
    if contains(name,'Mixer')
        num_outputs(Iclass) = 1;
    end
end

end %end function


%% %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%5
function all_categories = guessCategory(class_names)


all_categories={};
for Iname=1:length(class_names)
    name = class_names{Iname};
    
    %strip off leading 'Audio'
    if strcmpi(name(1:5),'Audio')
        name = name(6:end);
    end
    
    %find the first capital letter *after* the leading capital
    expr = '[A-Z]'; capStartIndex = regexp(name,expr);
    if isempty(capStartIndex);
        Icap=length(name)+1;
    else
        if capStartIndex(1) == 1;
            Icap = capStartIndex(2);
        else
            Icap = capStartIndex(1);
        end;
    end
    
    expr = '[0-9]'; numStartIndex = regexp(name,expr);
    if isempty(numStartIndex)
        Inum = length(name)+1;
    else
        Inum = numStartIndex(1);
    end
    
    I = find(name == '_');
    if isempty(I)
        Iund = length(name)+1;
    else
        Iund = I(1);
    end
    
    %get the category
    name = name(1:(min([Icap Inum Iund])-1));
    name = lower(name);  %convert to lower case
    
    %translate the category name to an existing one
    switch lower(name)
        case 'synth'
            name = 'synth';
        case 'multiply'
            name = 'effect';
        case 'divide'
            name = 'effect';
    end
    
    %append "function"
    name = [name '-function'];
    
    %save the name
    all_categories{Iname} = name;
end
end %end function 


%% %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function all_icons = chooseIcon(all_class_names,all_categories)

all_icons = {};
for Iclass = 1:length(all_class_names)
    category = all_categories(Iclass);
    icon = 'arrow-in.png';  %default
    
    if strcmpi(lower(category),'control-function')
        icon = 'debug.png';
    end
    
    all_icons{end+1} = icon;
end
end %end function