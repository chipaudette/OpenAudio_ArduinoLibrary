function all_lines = readAllLines(fname)

fid=fopen(fname,'r');
all_lines=[];
tline=fgetl(fid);
while ischar(tline)
    all_lines{end+1} = tline;
    tline=fgetl(fid);
end
fclose(fid);
