/**
 * The output mapper builds a relation between output groups (or a collection of
 * groups, called an ubergroup) and an effect routine.
 */
#ifndef OUTPUTMAPPER_H
#define OUTPUTMAPPER_H

#include "HSIPixel.h"
#include "db/Group.h"

#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <exception>

#include "INIReader.h"

class Routine;

class DataStore;
class Framebuffer;

class OutputMapper {
	friend class EffectRunner;

	public:
		struct invalid_ubergroup : public std::exception {
			const char *what() const throw () {
		    	return "Invalid ubergroup";
		    }
		};

	public:
		class OutputGroup {
			friend class OutputMapper;

			public:
				OutputGroup() = delete;
				OutputGroup(DbGroup *g);
				virtual ~OutputGroup();

				/**
				 * Returns an iterator into the group's framebuffer.
				 */
				HSIPixel *getDataPointer() {
					return this->buffer;
				}

				virtual int numPixels();

				virtual void bindBufferToRoutine(Routine *r);
				virtual void copyIntoFramebuffer(Framebuffer *fb, HSIPixel *buffer = nullptr);

			private:
				virtual void _resizeBuffer();

				bool bufferChanged = false;
				Routine *bufferBoundRoutine = nullptr;

				HSIPixel *buffer = nullptr;
				size_t bufferSz = 0;

			private:
				DbGroup *group = nullptr;

				friend bool operator==(const OutputGroup& lhs, const OutputGroup& rhs);
				friend bool operator< (const OutputGroup& lhs, const OutputGroup& rhs);

				friend std::ostream &operator<<(std::ostream& strm, const OutputGroup& obj);
		};

		class OutputUberGroup: public OutputGroup {
			friend class OutputMapper;

			public:
				OutputUberGroup();
				OutputUberGroup(std::vector<OutputGroup *> &members);
				~OutputUberGroup();

			public:
				// overrides from OutputGroup
				virtual int numPixels();

				virtual void copyIntoFramebuffer(Framebuffer *fb, HSIPixel *buffer = nullptr);

				int numMembers() {
					return this->groups.size();
				}

			private:
				// virtual void _resizeBuffer();

				void addMember(OutputGroup *group);
				void removeMember(OutputGroup *group);
				bool containsMember(OutputGroup *group);

			private:
				// std::recursive_mutex groupsLock;

				std::set<OutputGroup *> groups;


			// operators
				friend bool operator==(const OutputUberGroup& lhs, const OutputUberGroup& rhs);
				friend bool operator< (const OutputUberGroup& lhs, const OutputUberGroup& rhs);

				friend std::ostream &operator<<(std::ostream& strm, const OutputUberGroup& obj);
		};

	public:
		OutputMapper(DataStore *s, Framebuffer *f, INIReader *reader);
		~OutputMapper();

	public:
		void addMapping(OutputGroup *g, Routine *r);
		void removeMappingForGroup(OutputGroup *g);

		inline Routine *routineForMapping(OutputGroup *g) {
			return this->outputMap[g];
		}

	private:
		void _removeMappingsInUbergroup(OutputUberGroup *ug);

		void removeEmptyUbergroups(void);

		void printMap(void);

	private:
		DataStore *store;
		Framebuffer *fb;
		INIReader *config;

		// TODO: indicate if the output config was changed

		std::recursive_mutex outputMapLock;
		std::map<OutputGroup *, Routine *> outputMap;
};

// operators
inline std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputGroup *obj) {
	strm << *obj;
	return strm;
}
inline std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputUberGroup *obj) {
	strm << *obj;
	return strm;
}

#endif
