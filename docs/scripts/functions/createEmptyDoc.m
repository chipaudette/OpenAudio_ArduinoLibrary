function all_lines = createEmptyDoc(name)

all_lines={};
all_lines{end+1} = ['<script type="text/x-red" data-template-name="' name ' ">'];
all_lines{end+1} = [sprintf('\t') '<div class="form-row">'];
all_lines{end+1} = [sprintf('\t\t') '<label for="node-input-name"><i class="fa fa-tag"></i> Name</label>'];
all_lines{end+1} = [sprintf('\t\t') '<input type="text" id="node-input-name" placeholder="Name">'];
all_lines{end+1} = [sprintf('\t') '</div>'];
all_lines{end+1} = '</script>';

