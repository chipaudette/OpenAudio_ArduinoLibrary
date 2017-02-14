function all_data = parseNodeFile(fname)

if nargin < 1
    fname = 'Temp\nodes.txt';
end

%% read file
fid=fopen(fname,'r');
all_lines=[];
tline=fgetl(fid);
while ischar(tline)
    all_lines{end+1} = tline;
    tline=fgetl(fid);
end
fclose(fid);

%% parse the file
all_data=[];
for Iline=1:length(all_lines)
    data=[];
    line = all_lines{Iline};
    
    %find start
    I=find(line == '{');
    if ~isempty(I);
        line=line(I(1)+1:end);
        
        %find end of field
        I=find(line == ',');
        field_count = 0;
        while ~isempty(I)
            %get entry and remove from line
            entry = line(1:I(1)-1);
            line = line(I(1)+1:end);
            
            %parse entry
            I=find(entry == ':');
            if ~isempty(I)
                field_count = field_count+1;
                fieldname = entry(1:I(1)-1);
                fieldname = fieldname(2:end-1); %remove quotes
                
                value = entry(I(1)+1:end);
                if (field_count == 2) & (fieldname == 'data')
                    expected_value = '{"defaults":{"name":{"value":"new"}}';
                    if (value(2+[1:length('defaults')])=='defaults')
                        %this is good.  "line" is OK.
                    else
                        %push value back onto line
                        line = [value(2:end) ',' line];
                    end
                    value = expected_value;
                else
                    value = stripOffStartEndQuotes(value);
                end
                
                %make correction if this was the last entry
                if isempty(line)
                    %strip off the curly braces
                    while (value(end) == '}'); value = value(1:end-1); end;
                    value = stripOffStartEndQuotes(value);
                end
                
                %save this value
                data.(fieldname) = value;
                
                
            end
            I=find(line == ',');
            if ~isempty(I)
                J=find(line == '"');
                if isempty(J)
                    %I is the comma at the end of the line, not a seperator of entries.
                    %reject it
                    I=[];
                end
            end 
        end
        
        %get the last entry
        I=find(line == '}');
        if ~isempty(I);
            entry = line(1:I(1)-1);
            I=find(entry == ':');
            
            fieldname = entry(1:I(1)-1);
            fieldname = fieldname(2:end-1); %remove quotes
            
            value = entry(I(1)+1:end);
            I = find(value == '"');
            if (length(I) >= 2)
                value = value(2:end-1); %remove quotes;
            end
            data.(fieldname) = value;
        end
        
        %save the data
        if isempty(all_data)
            all_data = data;
        else
            all_data(end+1) = data;
        end
    end
end

return

%% %%%%%%%%%%%%%%%%%%%%%%%%%%%%5555
function value = stripOffStartEndQuotes(value)

I = find(value == '"');
if length(I) >= 2
    if (I(1) == 1) & (I(end)==length(value))
        value = value(2:end-1); %remove quotes;
    end
end
