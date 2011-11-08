// This file is released under the 3-clause BSD license. See COPYING-BSD.
// Generated by builder.sce : Please, do not edit this file
// cleaner.sce
// ------------------------------------------------------
curdir = pwd();
cleaner_path = get_file_path('cleaner.sce');
chdir(cleaner_path);
// ------------------------------------------------------
if fileinfo('loader.sce') <> [] then
  mdelete('loader.sce');
end
// ------------------------------------------------------
if fileinfo('Makelib.mak') <> [] then
  unix_s('nmake /Y /nologo /f Makelib.mak clean');
  mdelete('Makelib.mak');
end
// ------------------------------------------------------
if fileinfo('libsp_get.dll') <> [] then
  mdelete('libsp_get.dll');
end
// ------------------------------------------------------
chdir(curdir);
// ------------------------------------------------------
