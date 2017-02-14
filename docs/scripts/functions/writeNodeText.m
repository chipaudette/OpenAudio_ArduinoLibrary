function writeNodeText(nodes,outfname)

fn = fieldnames(nodes(1));

%outfname = 'NewOutputs\new_nodes.txt';
disp(['writing new node info to ' outfname]);
fid=fopen(outfname,'w');
for Inode=1:length(nodes)
    fprintf(fid,'		{');
    for Ifn=1:length(fn)
        field_name = fn{Ifn};
        fprintf(fid,'"%s":',field_name);
        
        field_val = nodes(Inode).(field_name);
        if field_val(1) == '"'; field_val = field_val(2:end); end;
        if field_val(end) == '"'; field_val = field_val(1:end-1);end;
        if strcmpi(field_name,'data')
            fprintf(fid,'%s',field_val);
        elseif isnumeric(field_val)
            fprintf(fid,'"%i"',field_val);
        else
            fprintf(fid,'"%s"',field_val);
        end
        
        if Ifn < length(fn)
            fprintf(fid,',');
        else
            if field_val(end-1:end) == '}}';
                %do nothing;
            else
                %insert these symbols
                fprintf(fid,'}}');
            end
        end
        
    end
    if Inode < length(nodes)
        fprintf(fid,',\n');
    else
        fprintf(fid,'\n');
    end
end
fclose(fid);
