
/**
   https://github.com/bstarynk/clips-rules-gcc

   file clips-gcc-plugin.cc driving the CLIPS engine. To be indented with
   astyle --style=gnu -s2 clips-gcc-plugin.cc

   Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)
   contributed by Basile Starynkevitch.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

**/
#include "clips-gcc.hh"

int plugin_is_GPL_compatible;

int clgcc_debug;
std::string CLGCC_projectstr;
std::string CLGCC_translationunitstr;
Environment* CLGCC_env;

// give elapsed process CPU time in seconds
double
CLGCC_cputime(void)
{
  struct timespec ts = {0,0};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts))
    return NAN;
  return (double) ts.tv_sec + 1.0e-9*ts.tv_nsec;
} // end CLGCC_cputime



const char*CLGCC_basename(const char* path)
{
  if (!path)
    return NULL;
  const char* lastsl = strrchr(path, '/');
  if (lastsl && lastsl[1])
    return lastsl+1;
  return path;
} // end of CLGCC_basename

void
CLGCC_dodbgprintf(const char*srcfil, int lin, const char*fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  fprintf(stderr, "%s:%d: ", CLGCC_basename(srcfil), lin);
  vfprintf(stderr, fmt, args);
  va_end(args);
  putc('\n', stderr);
  fflush(NULL);
} // end CLGCC_dodbgprintf

/// GCC callback which gets called before processing a translation unit
void
CLGCC_starting(void*gccdata __attribute__((unused)), void*userdata __attribute((unused)))
{
  char cputimbuf[32];
  memset (cputimbuf, 0, sizeof(cputimbuf));
  snprintf(cputimbuf, sizeof (cputimbuf), "%.3f s", CLGCC_cputime());
  inform(UNKNOWN_LOCATION, "CLIPS-GCC: ****starting project %s translunit %s, cputime %s (l@%d)",
         CLGCC_projectstr.c_str(),
         CLGCC_translationunitstr.c_str(),
         cputimbuf, __LINE__);
} // end CLGCC_starting


/// GCC callback which gets called near end, and may be useful for summary processing.
void
CLGCC_finishing(void*gccdata __attribute__((unused)), void*userdata __attribute((unused)))
{
  char cputimbuf[32];
  memset (cputimbuf, 0, sizeof(cputimbuf));
  snprintf(cputimbuf, sizeof (cputimbuf), "%.3f s", CLGCC_cputime());
  inform(UNKNOWN_LOCATION, "CLIPS-GCC: ****finishing project %s translunit %s, cputime %s (l@%d)",
         CLGCC_projectstr.c_str(),
         CLGCC_translationunitstr.c_str(),
         cputimbuf, __LINE__);
  if (!CL_DestroyEnvironment(CLGCC_env))
    fatal_error(UNKNOWN_LOCATION, "CLIPS-GCC: CL_DestroyEnvironment failed");

} // end CLGCC_finishing



void
parse_plugin_arguments (const char*plugin_name, struct plugin_name_args* plargs, std::deque<std::function<void(void)>>& todoque)
{
  int plargc = plargs->argc;
  int ix= 0;
  static char versbuf[256];
  snprintf(versbuf, sizeof(versbuf),
           "CLIPS-GCC plugin %s, built %s",
           clgcc_lastgitcommit, clgcc_timestamp);
  assert (plargs->version == NULL);
  assert (plargs->help == NULL);
  plargs->version = versbuf;
  plargs->help = "See https://github.com/bstarynk/clips-rules-gcc";
  auto str_plugin_name = std::string(plugin_name);
  //
  for (struct plugin_argument* plcurarg = plargs->argv;
       (ix<plargc)?(plcurarg = plargs->argv+ix):nullptr; ix++)
    {
      const char*curkey = plcurarg->key;
      const char*curval = plcurarg->value;
#define CLGCC_GOT_OPTION(S)   (!strcmp(curkey, (S)) && curval)
#define CLGCC_GOT_PLAIN_OPTION(S)   (!strcmp(curkey, (S)) && !curval)
#define CLGCC_GOT_ANY_OPTION(S)   (!strcmp(curkey, (S)))
      if (CLGCC_GOT_OPTION("project"))
        {
          CLGCC_projectstr = std::string(curval);
        } // end CLGCC_GOT_OPTION("project")
      ////////////////
      else if (CLGCC_GOT_PLAIN_OPTION("help"))
        {
          inform (UNKNOWN_LOCATION,
                  "CLIPS-GCC plugin %s help:\n", plugin_name);
          printf("\t -fplugin-arg-%s-help #this help\n", plugin_name);
          printf("\t -fplugin-arg-%s-project=<projectname> or $CLIPSGCC_PROJECT\n", plugin_name);
        } // end (CLGCC_GOT_PLAIN_OPTION("help")
      ////////////////
      else if (CLGCC_GOT_OPTION("load"))
        {
          if (access(curval, R_OK))
            warning(UNKNOWN_LOCATION, "CLIPS-GCC: plugin %s with bad CLIPS file %s to load (%m)",
                    plugin_name, curval);
          else
            {
              CLGCC_DBGPRINTF("CLIPS-GCC: plugin %s will load %s", plugin_name, curval);
              std::string curpath(curval);
              todoque.push_back([=]()
              {
                CLGCC_DBGPRINTF("CLIPS-GCC: plugin %s loading %s",  str_plugin_name.c_str(), curpath.c_str());
                if (CL_Load(CLGCC_env, curpath.c_str()))
                  warning(UNKNOWN_LOCATION,"CLIPS-GCC: plugin %s failed to load CLIPS file %s",
                          str_plugin_name.c_str(), curpath.c_str());
                else
                  inform(UNKNOWN_LOCATION, "CLIPS-GCC plugin %s did load CLIPS file %s",
                         str_plugin_name.c_str(), curpath.c_str());
              });
            }
        } // end CLGCC_GOT_OPTION("load")
      ////////////////
      else if (CLGCC_GOT_ANY_OPTION("debug"))
        {
          const char*dbgstr = curval;
          if (dbgstr)
            {
              inform (UNKNOWN_LOCATION,
                      "CLIPS-GCC plugin %s debug given %s:\n", plugin_name, dbgstr);
              if (!strcmp(dbgstr, "0") || !strcmp(dbgstr, "no"))
                {
                  inform (UNKNOWN_LOCATION,
                          "CLIPS-GCC plugin %s debug explicitly disabled", plugin_name);
                  clgcc_debug = 0;
                }
              else if (clgcc_debug > 0)
                {
                  int oldbg = clgcc_debug;
                  clgcc_debug = atoi(dbgstr);
                  if (oldbg != clgcc_debug)
                    inform (UNKNOWN_LOCATION,
                            "CLIPS-GCC plugin %s debug was %d now %d", plugin_name, oldbg, clgcc_debug);
                }
              else
                {
                  clgcc_debug = atoi(dbgstr);
                  inform (UNKNOWN_LOCATION,
                          "CLIPS-GCC plugin %s debug set to #%d", plugin_name, clgcc_debug);
                }
            }
          else
            {
              clgcc_debug = 1;
              inform (UNKNOWN_LOCATION,
                      "CLIPS-GCC plugin %s debug level set to one", plugin_name);
            }
        } // end (CLGCC_GOT_ANY_OPTION("debug")
      else
        {
          if (curval)
            warning(UNKNOWN_LOCATION, "CLIPS-GCC: unexpected plugin %s argument %s=%s",
                    plugin_name, curkey, curval);
          else
            warning(UNKNOWN_LOCATION, "CLIPS-GCC: unexpected plugin %s argument %s",
                    plugin_name, curkey);
        }
#undef CLGCC_GOT_OPTION
#undef CLGCC_GOT_PLAIN_OPTION
#undef CLGCC_GOT_ANY_OPTION
    }
} // end parse_plugin_arguments

////////////////////////////////////////////////////////////////
int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  const char *plugin_name = plugin_info->base_name;
  std::deque<std::function<void(void)>> todoque;
  if (!plugin_default_version_check (version, &gcc_version))
    {
      warning(UNKNOWN_LOCATION, "CLIPS-GCC plugin: fail version check for %s:\n"
              " got %s/%s/%s/%s wanted %s/%s/%s/%s", plugin_name,
              version->basever, version->datestamp, version->devphase, version->revision,
              gcc_version.basever, gcc_version.datestamp, gcc_version.devphase, gcc_version.revision);
      return 1;
    }
  inform(UNKNOWN_LOCATION, "CLIPS-GCC plugin %s starting (build %s git commit %s)",
         plugin_name, clgcc_timestamp, clgcc_lastgitcommit);
  auto dbgstr = getenv("CLIPSGCC_DEBUG");
  if (dbgstr)
    {
      clgcc_debug = atoi(dbgstr);
      if (clgcc_debug > 0)
        {
          inform(UNKNOWN_LOCATION, "CLIPS-GCC plugin %s enabled debugging #%d with CLIPSGCC_DEBUG=%s",
                 plugin_name, clgcc_debug, dbgstr);
        }
      else
        warning(UNKNOWN_LOCATION, "CLIPS-GCC plugin %s: CLIPSGCC_DEBUG is %s but disabled debugging",
                plugin_name, dbgstr);
    }
  /// the CLIPS environment needs to be created very early
  CLGCC_env = CL_CreateEnvironment();
  if (!CLGCC_env)
    fatal_error(UNKNOWN_LOCATION, "CLIPS-GCC: CL_CreateEnvironment failed");
  CLGCC_DBGPRINTF("plugin_init %s created CLGCC_env@%p", plugin_name, CLGCC_env);
  CLGCC_DBGPRINTF("plugin_init %s before registering", plugin_name);
  register_callback (plugin_name, PLUGIN_START_UNIT, CLGCC_starting, NULL);
  register_callback (plugin_name, PLUGIN_FINISH_UNIT, CLGCC_finishing, NULL);
  /// initialize global state from arguments, and give information about this plugin
  CLGCC_DBGPRINTF("plugin_init %s before parsing arguments", plugin_name);
  parse_plugin_arguments (plugin_name, plugin_info, todoque);
  CLGCC_DBGPRINTF("plugin_init %s before todo todoquelength=%zd", plugin_name, todoque.size());
  for (auto todof: todoque)
    todof();

  warning(UNKNOWN_LOCATION, "CLIPS-GCC plugin: unimplemented plugin_init");
#warning CLIPS-GCC plugin: unimplemented plugin_init
  return 0;
} // end plugin_init



////////////////////////////////////////////////////////////////

// end of file clips-gcc-plugin.cc
